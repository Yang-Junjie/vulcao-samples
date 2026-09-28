#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "common/window.h"
#include "vulcao/buffer.h"
#include "vulcao/command_buffer.h"
#include "vulcao/context.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/frame_manager.h"
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

struct Vertex {
    float position[3];
    float color[3];
};

const std::array<Vertex, 3> triangle_vertices{{
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
}};

const std::array<uint16_t, 3> triangle_indices{{0, 1, 2}};

std::filesystem::path shader_path(const char* name) {
    return std::filesystem::path(VULCAO_SHADER_DIR) / name;
}

}

int main() {
    vulcao::set_log_level(vulcao::LogLevel::info);
    vulcao::set_log_callback([](const vulcao::LogMessage& message) {
        std::cout << '[' << vulcao::to_string(message.level) << ": "
                  << vulcao::to_string(message.category) << "] " << message.message << std::endl;
    });

    try {
        sample::Window window{800, 600, "02_uniforms"};

        vulcao::Context context{{.app_name = "02_uniforms"}};
        context.initialize(window.create_surface(context.instance()), window.framebuffer_extent());

        const vulcao::ShaderModule vertex = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eVertex, shader_path("uniforms.vert.spv"));
        const vulcao::ShaderModule fragment = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eFragment, shader_path("uniforms.frag.spv"));

        vulcao::Buffer vertex_buffer = vulcao::Buffer::create_with_data(
            context, triangle_vertices, vk::BufferUsageFlagBits::eVertexBuffer);
        vulcao::Buffer index_buffer = vulcao::Buffer::create_with_data(
            context, triangle_indices, vk::BufferUsageFlagBits::eIndexBuffer);

        const vulcao::VertexLayout vertex_layout = vulcao::make_vertex_layout<Vertex>(
            vertex.reflection(),
            {offsetof(Vertex, position), offsetof(Vertex, color)});

        const std::array<vulcao::ShaderReflection, 2> reflections{vertex.reflection(),
                                                                  fragment.reflection()};
        const vulcao::PipelineLayout layout =
            vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);

        const vulcao::Pipeline pipeline = vulcao::Pipeline::create_graphics(
            context.device(), layout,
            vulcao::GraphicsPipelineInfo{
                .vertex_shader = vertex.handle(),
                .fragment_shader = fragment.handle(),
                .vertex_entry = "vertMain",
                .fragment_entry = "fragMain",
                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .vertex_bindings = vertex_layout.bindings,
                .vertex_attributes = vertex_layout.attributes,
                .color_formats = {context.swapchain_format()},
            });

        vulcao::FrameManager frames{context};
        const uint32_t frame_count = frames.frames_in_flight();

        std::vector<vulcao::Buffer> uniform_buffers;
        uniform_buffers.reserve(frame_count);
        for (uint32_t i = 0; i < frame_count; ++i)
            uniform_buffers.push_back(vulcao::Buffer::create(
                context.allocator(), sizeof(glm::mat4), vk::BufferUsageFlagBits::eUniformBuffer,
                VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT));

        // The pool is sized from the reflected bindings, one set per frame.
        vulcao::DescriptorPool descriptor_pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), vertex.reflection().bindings_for_set(0), frame_count);

        std::vector<vulcao::DescriptorSet> descriptor_sets;
        descriptor_sets.reserve(frame_count);
        for (uint32_t i = 0; i < frame_count; ++i) {
            vulcao::DescriptorSet set = descriptor_pool.allocate(layout.set_layout(0));
            set.write_uniform_buffer(0, uniform_buffers[i]);
            descriptor_sets.push_back(set);
        }

        const double start_time = glfwGetTime();

        auto render = [&](double time_seconds) {
            vulcao::Frame frame = frames.begin_frame();
            vulcao::CommandBuffer& cmd = *frame.command_buffer;
            const uint32_t slot = frame.slot;

            const glm::mat4 mvp = glm::rotate(glm::mat4(1.0f), static_cast<float>(time_seconds),
                                              glm::vec3(0.0f, 0.0f, 1.0f));
            uniform_buffers[slot].write_bytes(&mvp, sizeof(mvp));

            const vk::Image image = context.swapchain_images()[frame.image_index];
            const vk::ImageView view = context.swapchain_image_views()[frame.image_index];
            const vk::Extent2D extent = context.swapchain_extent();

            cmd.transition_to_render(image);
            cmd.begin_rendering(
                extent,
                vulcao::color_attachment(view, vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::ClearColorValue{
                                             std::array<float, 4>{0.05f, 0.10f, 0.20f, 1.0f}}));
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, layout.handle(),
                                     descriptor_sets[slot].handle());
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertex_buffer);
            cmd.bind_index_buffer(index_buffer, 0, vk::IndexType::eUint16);
            cmd.draw_indexed(static_cast<uint32_t>(triangle_indices.size()));
            cmd.end_rendering();
            cmd.transition_to_present(image);

            frames.end_frame(frame);
            return frames.present(frame);
        };

        auto recreate_swapchain = [&]() {
            const vk::Extent2D extent = window.framebuffer_extent();
            if (extent.width == 0 || extent.height == 0)
                return false;
            frames.recreate_swapchain(extent);
            return true;
        };

        while (!window.should_close()) {
            window.poll_events();

            if (window.consume_resized()) {
                if (!recreate_swapchain())
                    continue;
            }

            try {
                if (!render(glfwGetTime() - start_time))
                    recreate_swapchain();
            } catch (const vk::OutOfDateKHRError&) {
                recreate_swapchain();
            }
        }

        context.wait_idle();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
