#include <array>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/sampler.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("15_texture_array", [] {
        vulcao::Context context{{.app_name = "15_texture_array", .validation = true, .headless = true}};
        context.initialize();
        constexpr uint32_t layer_size = 4;
        constexpr uint32_t layer_count = 3;
        constexpr vk::Extent2D extent{96, 32};
        auto texture = vulcao::Image::create(context.allocator(), vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = sample::color_format,
            .extent = {layer_size, layer_size, 1},
            .mipLevels = 1,
            .arrayLayers = layer_count,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        });
        // Image::create infers a 2D-array view spanning all three layers.
        std::vector<sample::Pixel> texels(layer_count * layer_size * layer_size);
        for (uint32_t layer = 0; layer < layer_count; ++layer)
            for (uint32_t i = 0; i < layer_size * layer_size; ++i) {
                sample::Pixel color{0, 0, 0, 255};
                color[layer] = 255;
                texels[layer * layer_size * layer_size + i] = color;
            }
        auto staging = vulcao::Buffer::create(context.allocator(), texels.size() * sizeof(sample::Pixel),
            vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        staging.write(texels);
        const auto sampler = vulcao::Sampler::nearest(context.device());
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "array.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "array.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), fragment.reflection().bindings_for_set(0), 1);
        const auto set = pool.allocate(layout.set_layout(0));
        // Separate descriptors let several images share a sampler.
        vulcao::DescriptorSetWriter(set).write_sampled_image(0, texture).write_sampler(1, sampler).flush();
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
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

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(texture, vk::ImageLayout::eTransferDstOptimal);
            for (uint32_t layer = 0; layer < layer_count; ++layer)
                cmd.copy_buffer_to_image(staging.handle(), texture.handle(), vk::BufferImageCopy{
                    .bufferOffset = layer * layer_size * layer_size * sizeof(sample::Pixel),
                    .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, layer, 1},
                    .imageExtent = {layer_size, layer_size, 1},
                });
            cmd.transition(texture, vk::ImageLayout::eShaderReadOnlyOptimal);
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.begin_rendering(extent, vulcao::color_attachment(target.view(), target.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}));
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, layout.handle(), set.handle());
            cmd.bind_vertex_buffer(0, vertices);
            for (uint32_t layer = 0; layer < layer_count; ++layer) {
                cmd.set_viewport(vk::Viewport{static_cast<float>(layer * 32), 0, 32, 32, 0, 1});
                cmd.set_scissor(vk::Rect2D{{static_cast<int32_t>(layer * 32), 0}, {32, 32}});
                cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eFragment, 0, layer);
                cmd.draw(3);
            }
            cmd.end_rendering();
        });
        const auto pixels = sample::read_pixels(context, target);
        for (uint32_t y = 0; y < extent.height; ++y)
            for (uint32_t x = 0; x < extent.width; ++x) {
                sample::Pixel expected{0, 0, 0, 255};
                expected[x / 32] = 255;
                sample::expect_pixel(pixels, extent.width, x, y, expected);
            }
        std::cout << "Verified per-layer uploads, a 2D-array view and separate image/sampler descriptors\n";
    });
}
