#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>

#include "common/window_loop.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main(int argc, char** argv) {
    return sample::run("22_deferred_destruction", [&] {
        const uint32_t limit = sample::frame_limit(argc, argv);
        sample::Window window{800, 600, "22_deferred_destruction - replace a buffer every frame"};
        vulcao::Context context{{.app_name = "22_deferred_destruction", .validation = true}};
        context.initialize(window.create_surface(context.instance()), window.framebuffer_extent());
        struct Vertex { float position[2]; float color[3]; };
        constexpr std::array<Vertex, 3> original{{
            {{0, -0.65f}, {1, 0.2f, 0.1f}},
            {{0.6f, 0.5f}, {0.1f, 1, 0.3f}},
            {{-0.6f, 0.5f}, {0.2f, 0.3f, 1}},
        }};
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "triangle.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "triangle.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(
            vertex.reflection(), {offsetof(Vertex, position), offsetof(Vertex, color)});
        const auto pipeline = vulcao::Pipeline::create_graphics(context.device(), layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {context.swapchain_format()},
        });
        uint32_t created = 0, retired = 0;
        vulcao::Buffer current;
        {
            vulcao::FrameManager frames{context, {.frames_in_flight = 3}};
            sample::run_window(window, context, frames, limit, [&](vulcao::Frame& frame, double time) {
                const auto extent = context.swapchain_extent();
                const float c = std::cos(static_cast<float>(time));
                const float s = std::sin(static_cast<float>(time));
                const float aspect = static_cast<float>(extent.width) / extent.height;
                auto animated = original;
                for (auto& point : animated) {
                    const float x = point.position[0], y = point.position[1];
                    point.position[0] = (c * x - s * y) / aspect;
                    point.position[1] = s * x + c * y;
                }
                // Host-visible memory avoids a synchronous staging submission here.
                auto replacement = vulcao::Buffer::create(context.allocator(), sizeof(animated),
                    vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
                replacement.write(animated);
                if (current) {
                    // Earlier frames can still reference the old buffer. Retire it
                    // only after this slot's fence signals on a future begin_frame.
                    frames.defer_destroy(frame, std::move(current));
                    frames.defer_destroy(frame, std::function<void()>{[&retired] { ++retired; }});
                }
                current = std::move(replacement);
                ++created;
                auto& cmd = *frame.command_buffer;
                const auto image = context.swapchain_images()[frame.image_index];
                cmd.transition_to_render(image);
                cmd.begin_rendering(extent, vulcao::color_attachment(
                    context.swapchain_image_views()[frame.image_index],
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ClearColorValue{std::array<float, 4>{0.02f, 0.03f, 0.05f, 1}}));
                cmd.bind_pipeline(pipeline);
                cmd.set_viewport(extent);
                cmd.set_scissor(extent);
                cmd.bind_vertex_buffer(0, current);
                cmd.draw(3);
                cmd.end_rendering();
                cmd.transition_to_present(image);
            });
        } // FrameManager waits and drains the remaining deletion queues.
        sample::require(retired == (created == 0 ? 0 : created - 1),
                        "not all replaced buffers were retired");
        std::cout << "Created " << created << " buffers, safely retired " << retired << "\n";
    });
}
