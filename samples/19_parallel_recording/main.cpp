#include <array>
#include <future>

#include "common/sample.h"
#include "vulcao/command_pool.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

namespace {

// Member order keeps the pool alive until its command buffer is freed.
struct Recording {
    vulcao::CommandPool pool;
    vulcao::CommandBuffer cmd;
};

}

int main() {
    return sample::run("19_parallel_recording", [] {
        vulcao::Context context{{.app_name = "19_parallel_recording",
                                 .validation = true, .headless = true}};
        context.initialize();
        constexpr uint32_t workers = 4;
        constexpr vk::Extent2D extent{128, 64};
        constexpr std::array<std::array<float, 4>, workers> colors{{
            {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1},
        }};
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "tile.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "tile.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        const auto pipeline = vulcao::Pipeline::create_graphics(context.device(), layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {sample::color_format},
        });
        auto target = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
        std::array<Recording, workers> recordings;
        std::array<std::future<Recording>, workers> pending;
        for (uint32_t worker = 0; worker < workers; ++worker) {
            pending[worker] = std::async(std::launch::async, [&, worker] {
                Recording recording;
                // Never record buffers from the same pool concurrently.
                recording.pool = vulcao::CommandPool::create(
                    context.device(), context.graphics_queue_family_index());
                recording.cmd = recording.pool.allocate(
                    vk::CommandBufferLevel::eSecondary, context.debug_utils_enabled());
                const vk::Format format = sample::color_format;
                const vk::CommandBufferInheritanceRenderingInfo rendering{
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &format,
                    .rasterizationSamples = vk::SampleCountFlagBits::e1,
                };
                recording.cmd.begin(vk::CommandBufferInheritanceInfo{.pNext = &rendering},
                    vk::CommandBufferUsageFlagBits::eOneTimeSubmit |
                    vk::CommandBufferUsageFlagBits::eRenderPassContinue);
                recording.cmd.begin_debug_label("Worker tile", colors[worker]);
                recording.cmd.bind_pipeline(pipeline);
                recording.cmd.set_viewport(extent);
                recording.cmd.set_scissor(vk::Rect2D{{static_cast<int32_t>(worker * 32), 0}, {32, 64}});
                recording.cmd.bind_vertex_buffer(0, vertices);
                recording.cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eFragment,
                                             0, colors[worker]);
                recording.cmd.draw(3);
                recording.cmd.end_debug_label();
                recording.cmd.end();
                return recording;
            });
        }
        for (uint32_t worker = 0; worker < workers; ++worker)
            recordings[worker] = pending[worker].get();

        // Image layout tracking and submission stay on the main thread.
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            const auto attachment = vulcao::color_attachment(target.view(), target.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}});
            cmd.begin_rendering(vk::RenderingInfo{
                .flags = vk::RenderingFlagBits::eContentsSecondaryCommandBuffers,
                .renderArea = {{0, 0}, extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attachment,
            });
            for (const auto& recording : recordings)
                cmd.execute_commands(recording.cmd.handle());
            cmd.end_rendering();
        });
        const auto pixels = sample::read_pixels(context, target);
        for (uint32_t y = 0; y < extent.height; ++y)
            for (uint32_t x = 0; x < extent.width; ++x) {
                const auto& color = colors[x / 32];
                sample::expect_pixel(pixels, extent.width, x, y, {
                    static_cast<uint8_t>(color[0] * 255), static_cast<uint8_t>(color[1] * 255),
                    static_cast<uint8_t>(color[2] * 255), 255});
            }
        std::cout << "Verified four secondary command buffers recorded on four worker threads\n";
    });
}
