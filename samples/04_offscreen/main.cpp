#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>

#include "vulcao/buffer.h"
#include "vulcao/command_buffer.h"
#include "vulcao/context.h"
#include "vulcao/image.h"
#include "vulcao/log.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/shader_module.h"
#include "vulcao/vertex_layout.h"

#ifndef VULCAO_SHADER_DIR
#define VULCAO_SHADER_DIR "shaders"
#endif

namespace {

constexpr uint32_t size = 256;

/// Counted by the log callback so a validation error fails the sample, which
/// keeps the result honest rather than only checking the pixels.
uint32_t validation_errors = 0;

constexpr std::array<float, 4> clear_color{0.0f, 0.0f, 0.0f, 1.0f};
constexpr std::array<float, 4> triangle_color{0.2f, 0.4f, 0.8f, 1.0f};

struct Vertex {
    float position[3];
};

/// A triangle that covers the middle of the image but leaves the corners alone.
constexpr std::array<Vertex, 3> triangle{{
    {{0.0f, -0.5f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}},
}};

/// The channel value the hardware writes for a UNORM target.
uint8_t unorm_channel(float value) {
    return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

constexpr std::array<uint8_t, 4> unorm_color(const std::array<float, 4>& color) {
    return {unorm_channel(color[0]), unorm_channel(color[1]), unorm_channel(color[2]),
            unorm_channel(color[3])};
}

/// Reads one pixel out of the tightly packed readback buffer.
std::array<uint8_t, 4> pixel_at(const uint8_t* pixels, uint32_t x, uint32_t y) {
    const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
    return {pixels[offset + 0], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

std::string describe(const std::array<uint8_t, 4>& color) {
    return "(" + std::to_string(color[0]) + ", " + std::to_string(color[1]) + ", " +
           std::to_string(color[2]) + ", " + std::to_string(color[3]) + ")";
}

}

int main() {
    vulcao::set_log_level(vulcao::LogLevel::info);
    vulcao::set_log_callback([](const vulcao::LogMessage& message) {
        if (message.category == vulcao::LogCategory::validation &&
            message.level == vulcao::LogLevel::error)
            ++validation_errors;

        std::cout << '[' << vulcao::to_string(message.level) << ": "
                  << vulcao::to_string(message.category) << "] " << message.message << std::endl;
    });

    try {
        // Rendering into an image needs no surface, so the context is created
        // headless and this sample runs on a machine without a display.
        vulcao::Context context{{.app_name = "04_offscreen", .headless = true}};
        context.initialize();

        const std::filesystem::path shader_dir = VULCAO_SHADER_DIR;
        const vulcao::ShaderModule vertex = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eVertex, shader_dir / "triangle.vert.spv");
        const vulcao::ShaderModule fragment = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eFragment, shader_dir / "triangle.frag.spv");

        // The vertex layout is derived from the reflection of the vertex shader.
        const vulcao::VertexLayout vertex_layout =
            vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {offsetof(Vertex, position)});

        vulcao::Buffer vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);

        // The render target. Sampled is what makes it usable as a texture once the
        // pass is done, and TransferSrc is what lets the sample read it back.
        vulcao::Image target = vulcao::Image::create_2d(
            context.allocator(), vk::Extent2D{size, size}, vk::Format::eR8G8B8A8Unorm,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eSampled);

        // The pipeline layout comes from the reflection of the fragment shader, so
        // the push constant range the shader declares is picked up automatically.
        const vulcao::PipelineLayout pipeline_layout =
            vulcao::PipelineLayout::create_from_reflection(
                context.device(), std::span(&fragment.reflection(), 1));

        const vulcao::Pipeline pipeline = vulcao::Pipeline::create_graphics(
            context.device(), pipeline_layout,
            vulcao::GraphicsPipelineInfo{
                .vertex_shader = vertex.handle(),
                .fragment_shader = fragment.handle(),
                .vertex_entry = "vertMain",
                .fragment_entry = "fragMain",
                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .vertex_bindings = vertex_layout.bindings,
                .vertex_attributes = vertex_layout.attributes,
                .color_formats = {vk::Format::eR8G8B8A8Unorm},
            });

        const vk::DeviceSize bytes = static_cast<vk::DeviceSize>(size) * size * 4;
        vulcao::Buffer readback = vulcao::Buffer::create(
            context.allocator(), bytes, vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

        // One submission: render into the image, copy it out, then leave it in the
        // layout a sampler expects.
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.begin_rendering(
                vk::Extent2D{size, size},
                vulcao::color_attachment(target.view(), target.layout(),
                                         vk::ClearColorValue{clear_color}));
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(vk::Extent2D{size, size});
            cmd.set_scissor(vk::Extent2D{size, size});
            cmd.push_constants(pipeline_layout.handle(), vk::ShaderStageFlagBits::eFragment, 0,
                               triangle_color);
            cmd.bind_vertex_buffer(0, vertices);
            cmd.draw(static_cast<uint32_t>(triangle.size()));
            cmd.end_rendering();

            // The transition doubles as the barrier that makes the attachment
            // writes visible to the copy.
            cmd.transition(target, vk::ImageLayout::eTransferSrcOptimal);
            cmd.copy_image_to_buffer(readback.handle(), target);
            cmd.transition(target, vk::ImageLayout::eShaderReadOnlyOptimal);
        });

        readback.invalidate();
        const auto* pixels = static_cast<const uint8_t*>(readback.map());

        // The triangle covers the middle of the image, the top left corner keeps
        // the clear colour.
        const std::array<uint8_t, 4> center = pixel_at(pixels, size / 2, size / 2);
        const std::array<uint8_t, 4> corner = pixel_at(pixels, 8, 8);
        const std::array<uint8_t, 4> expected_center = unorm_color(triangle_color);
        const std::array<uint8_t, 4> expected_corner = unorm_color(clear_color);

        if (center != expected_center) {
            std::cerr << "centre pixel is " << describe(center) << ", expected "
                      << describe(expected_center) << std::endl;
            return 1;
        }
        if (corner != expected_corner) {
            std::cerr << "corner pixel is " << describe(corner) << ", expected "
                      << describe(expected_corner) << std::endl;
            return 1;
        }

        if (validation_errors != 0) {
            std::cerr << validation_errors << " validation error(s) were reported" << std::endl;
            return 1;
        }

        std::cout << "rendered a " << size << "x" << size << " image, centre " << describe(center)
                  << " and corner " << describe(corner) << " as expected; left in "
                  << vk::to_string(target.layout()) << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
