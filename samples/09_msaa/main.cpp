#include <array>

#include "common/sample.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("09_msaa", [] {
        vulcao::Context context{{.app_name = "09_msaa", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{128, 128};
        const auto properties = context.physical_device().getImageFormatProperties(
            sample::color_format, vk::ImageType::e2D, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment, {});
        const auto supported = properties.sampleCounts &
            context.physical_device().getProperties().limits.framebufferColorSampleCounts;
        vk::SampleCountFlagBits samples;
        if (supported & vk::SampleCountFlagBits::e4)
            samples = vk::SampleCountFlagBits::e4;
        else if (supported & vk::SampleCountFlagBits::e2)
            samples = vk::SampleCountFlagBits::e2;
        else
            throw sample::Unsupported("RGBA8 color attachments support neither 2x nor 4x MSAA");

        auto multisample = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment, 1, samples);
        auto resolved = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{
            {{-0.83f, -0.74f}}, {{0.73f, -0.68f}}, {{-0.18f, 0.87f}},
        }};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "triangle.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "triangle.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        const auto pipeline = vulcao::Pipeline::create_graphics(context.device(), layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .samples = samples,
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {sample::color_format},
        });
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(multisample, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(resolved, vk::ImageLayout::eColorAttachmentOptimal);
            auto attachment = vulcao::color_attachment(multisample.view(), multisample.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}});
            attachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
            attachment.resolveImageView = resolved.view();
            attachment.resolveImageLayout = resolved.layout();
            // Only the single-sample resolve is needed after the pass.
            attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
            cmd.begin_rendering(extent, attachment);
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            cmd.draw(3);
            cmd.end_rendering();
        });
        const auto pixels = sample::read_pixels(context, resolved);
        sample::expect_pixel(pixels, extent.width, 64, 64, {255, 255, 255, 255});
        sample::expect_pixel(pixels, extent.width, 0, 0, {0, 0, 0, 255});
        size_t edge_pixels = 0;
        for (size_t i = 0; i < pixels.size(); i += 4)
            if (pixels[i] > 0 && pixels[i] < 255)
                ++edge_pixels;
        sample::require(edge_pixels > 0, "MSAA resolve produced no partially covered edge pixels");
        std::cout << static_cast<uint32_t>(samples) << "x MSAA: " << edge_pixels
                  << " partially covered edge pixels verified\n";
    });
}
