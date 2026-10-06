#include <array>

#include "common/sample.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

namespace {

struct Vertex { float position[2]; };
struct ObjectData { std::array<float, 4> color; float depth; float padding[3]{}; };

}

int main() {
    return sample::run("08_depth_blending", [] {
        vulcao::Context context{{.app_name = "08_depth_blending", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{128, 128};
        constexpr std::array<Vertex, 6> quad{{
            {{-0.8f, -0.8f}}, {{0.8f, -0.8f}}, {{0.8f, 0.8f}},
            {{-0.8f, -0.8f}}, {{0.8f, 0.8f}}, {{-0.8f, 0.8f}},
        }};
        auto vertices = vulcao::Buffer::create_with_data(
            context, quad, vk::BufferUsageFlagBits::eVertexBuffer);
        auto color = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
        auto depth = vulcao::Image::create_depth(context.allocator(), extent);
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "layers.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "layers.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        vulcao::GraphicsPipelineInfo info{
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .depth_test = true,
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {sample::color_format},
            .depth_format = depth.format(),
        };
        const auto opaque = vulcao::Pipeline::create_graphics(context.device(), layout, info);
        info.depth_write = false;
        info.blend = true;
        const auto transparent = vulcao::Pipeline::create_graphics(context.device(), layout, info);

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(color, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(depth, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            const auto depth_attachment = vulcao::depth_attachment(depth.view(), depth.layout(), 1.0f);
            cmd.begin_rendering(extent, vulcao::color_attachment(color.view(), color.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}), &depth_attachment);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            auto draw = [&](ObjectData object) {
                cmd.push_constants(layout.handle(),
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, object);
                cmd.draw(6);
            };
            cmd.bind_pipeline(opaque);
            draw({{1, 0, 0, 1}, 0.3f});
            draw({{0, 0, 1, 1}, 0.7f}); // Farther blue must fail depth testing.

            cmd.bind_pipeline(transparent);
            cmd.set_scissor(vk::Rect2D{{32, 32}, {64, 64}});
            draw({{0, 1, 0, 0.5f}, 0.2f});
            draw({{0, 0, 1, 0.5f}, 0.1f}); // Transparent layers, back to front.
            // A layer behind the opaque surface must still fail the depth test.
            draw({{1, 1, 1, 0.5f}, 0.8f});
            cmd.end_rendering();
        });
        const auto pixels = sample::read_pixels(context, color);
        sample::expect_pixel(pixels, extent.width, 64, 64, {64, 64, 128, 128});
        sample::expect_pixel(pixels, extent.width, 20, 64, {255, 0, 0, 255});
        sample::expect_pixel(pixels, extent.width, 2, 2, {0, 0, 0, 255});
        std::cout << "Verified opaque depth rejection, alpha blending and scissor clipping\n";
    });
}
