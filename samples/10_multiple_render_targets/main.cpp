#include <array>
#include <vector>

#include "common/sample.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("10_multiple_render_targets", [] {
        vulcao::Context context{{.app_name = "10_multiple_render_targets",
                                 .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{96, 96};
        constexpr auto usage = vk::ImageUsageFlagBits::eColorAttachment |
                               vk::ImageUsageFlagBits::eTransferSrc;
        auto albedo = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format, usage);
        auto normals = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format, usage);
        auto ids = vulcao::Image::create_2d(context.allocator(), extent, vk::Format::eR32Uint, usage);
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{
            {{0.0f, -0.8f}}, {{0.8f, 0.8f}}, {{-0.8f, 0.8f}},
        }};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "gbuffer.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "gbuffer.frag.spv");
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
            // Attachment order matches SV_Target0, SV_Target1 and SV_Target2.
            .color_formats = {albedo.format(), normals.format(), ids.format()},
        });
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(albedo, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(normals, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(ids, vk::ImageLayout::eColorAttachmentOptimal);
            const vk::ClearColorValue black{std::array<float, 4>{0, 0, 0, 0}};
            const std::array attachments{
                vulcao::color_attachment(albedo.view(), albedo.layout(), black),
                vulcao::color_attachment(normals.view(), normals.layout(), black),
                vulcao::color_attachment(ids.view(), ids.layout(),
                    vk::ClearColorValue{std::array<uint32_t, 4>{0, 0, 0, 0}}),
            };
            cmd.begin_rendering(extent, attachments);
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            cmd.draw(3);
            cmd.end_rendering();
        });
        const auto color_pixels = sample::read_pixels(context, albedo);
        const auto normal_pixels = sample::read_pixels(context, normals);
        sample::expect_pixel(color_pixels, extent.width, 48, 48, {64, 128, 191, 255});
        sample::expect_pixel(normal_pixels, extent.width, 48, 48, {128, 128, 255, 51});
        sample::expect_pixel(color_pixels, extent.width, 0, 0, {0, 0, 0, 0});
        sample::expect_pixel(normal_pixels, extent.width, 0, 0, {0, 0, 0, 0});
        std::vector<uint32_t> object_ids(extent.width * extent.height);
        context.download(ids, object_ids);
        sample::require(object_ids[48 * extent.width + 48] == 42 && object_ids[0] == 0,
                        "integer object-picking attachment did not match the scene");
        std::cout << "Verified albedo, encoded normal/roughness and integer object ID attachments\n";
    });
}
