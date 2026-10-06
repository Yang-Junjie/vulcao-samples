#include <array>
#include <cstddef>
#include <vector>

#include "common/window_loop.h"
#include "vulcao/buffer.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"

namespace {

struct Vertex { float position[2]; };
struct Instance { float offset[2]; float color[3]; };
struct FrameData { float time; float aspect; };

}

int main(int argc, char** argv) {
    return sample::run("07_instancing", [&] {
        const uint32_t limit = sample::frame_limit(argc, argv);
        sample::Window window{960, 640, "07_instancing - 96 instances, one draw"};
        vulcao::Context context{{.app_name = "07_instancing", .validation = true}};
        context.initialize(window.create_surface(context.instance()), window.framebuffer_extent());

        constexpr std::array<Vertex, 3> triangle{{
            {{0.0f, -0.075f}}, {{0.065f, 0.055f}}, {{-0.065f, 0.055f}},
        }};
        std::vector<Instance> instances;
        for (uint32_t y = 0; y < 8; ++y)
            for (uint32_t x = 0; x < 12; ++x)
                instances.push_back({{-1.15f + 0.21f * x, -0.78f + 0.22f * y},
                                     {0.2f + 0.8f * x / 11.0f, 0.2f + 0.8f * y / 7.0f, 0.8f}});
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        auto instance_buffer = vulcao::Buffer::create_with_data(
            context, instances, vk::BufferUsageFlagBits::eVertexBuffer);

        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "instances.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "instances.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto pipeline = vulcao::Pipeline::create_graphics(context.device(), layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            // Binding 0 advances per vertex, binding 1 advances per instance.
            .vertex_bindings = {
                {0, sizeof(Vertex), vk::VertexInputRate::eVertex},
                {1, sizeof(Instance), vk::VertexInputRate::eInstance},
            },
            .vertex_attributes = {
                {0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)},
                {1, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)},
                {2, 1, vk::Format::eR32G32B32Sfloat, offsetof(Instance, color)},
            },
            .color_formats = {context.swapchain_format()},
        });

        vulcao::FrameManager frames{context};
        sample::run_window(window, context, frames, limit, [&](vulcao::Frame& frame, double time) {
            auto& cmd = *frame.command_buffer;
            const auto extent = context.swapchain_extent();
            const auto image = context.swapchain_images()[frame.image_index];
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition_to_render(image);
            cmd.begin_rendering(extent, vulcao::color_attachment(
                context.swapchain_image_views()[frame.image_index],
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ClearColorValue{std::array<float, 4>{0.015f, 0.025f, 0.04f, 1.0f}}));
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eVertex, 0,
                               FrameData{static_cast<float>(time),
                                         static_cast<float>(extent.width) / extent.height});
            cmd.bind_vertex_buffer(0, vertices);
            cmd.bind_vertex_buffer(1, instance_buffer);
            cmd.draw(3, static_cast<uint32_t>(instances.size()));
            cmd.end_rendering();
            cmd.transition_to_present(image);
        });
    });
}
