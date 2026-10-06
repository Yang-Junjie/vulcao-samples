#include <array>
#include <cmath>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/sampler.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("11_storage_image", [] {
        vulcao::Context context{{.app_name = "11_storage_image", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{73, 51};
        auto generated = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
        auto target = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
        const auto compute = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                 "generate.comp.spv");
        const auto compute_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&compute.reflection(), 1));
        const auto compute_pipeline = vulcao::Pipeline::create_compute(
            context.device(), compute_layout, compute, "compMain");
        auto compute_pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), compute.reflection().bindings_for_set(0), 1);
        const auto compute_set = compute_pool.allocate(compute_layout.set_layout(0));
        compute_set.write_storage_image(0, generated);

        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "display.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "display.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto graphics_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), reflections);
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        const auto graphics_pipeline = vulcao::Pipeline::create_graphics(context.device(), graphics_layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {sample::color_format},
        });
        const auto sampler = vulcao::Sampler::nearest(context.device());
        auto graphics_pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), fragment.reflection().bindings_for_set(0), 1);
        const auto graphics_set = graphics_pool.allocate(graphics_layout.set_layout(0));
        graphics_set.write_image(0, generated, sampler);

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(generated, vk::ImageLayout::eGeneral);
            cmd.bind_pipeline(compute_pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, compute_layout.handle(),
                                     compute_set.handle());
            cmd.push_constants(compute_layout.handle(), vk::ShaderStageFlagBits::eCompute, 0,
                               std::array<uint32_t, 2>{extent.width, extent.height});
            cmd.dispatch((extent.width + 7) / 8, (extent.height + 7) / 8);

            // Layout transition also makes compute writes visible to fragment sampling.
            cmd.transition(generated, vk::ImageLayout::eShaderReadOnlyOptimal,
                           vk::PipelineStageFlagBits2::eComputeShader,
                           vk::AccessFlagBits2::eShaderStorageWrite,
                           vk::PipelineStageFlagBits2::eFragmentShader,
                           vk::AccessFlagBits2::eShaderSampledRead);
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.begin_rendering(extent, vulcao::color_attachment(target.view(), target.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}));
            cmd.bind_pipeline(graphics_pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, graphics_layout.handle(),
                                     graphics_set.handle());
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            cmd.draw(3);
            cmd.end_rendering();
        });
        const auto pixels = sample::read_pixels(context, target);
        for (uint32_t y = 0; y < extent.height; ++y)
            for (uint32_t x = 0; x < extent.width; ++x)
                sample::expect_pixel(pixels, extent.width, x, y, {
                    static_cast<uint8_t>(std::lround(255.0 * x / (extent.width - 1))),
                    static_cast<uint8_t>(std::lround(255.0 * y / (extent.height - 1))),
                    static_cast<uint8_t>((((x / 7) + (y / 5)) & 1) * 255), 255});
        std::cout << "Verified every pixel after compute storage writes and fragment sampling\n";
    });
}
