#include <array>
#include <cmath>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/sampler.h"

int main() {
    return sample::run("24_cubemap", [] {
        vulcao::Context context{{.app_name = "24_cubemap", .validation = true, .headless = true}};
        context.initialize();
        constexpr uint32_t side = 4;
        // Vulkan cube layers: +X, -X, +Y, -Y, +Z, -Z.
        constexpr std::array<sample::Pixel, 6> colors{{
            {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255},
            {255, 255, 0, 255}, {255, 0, 255, 255}, {0, 255, 255, 255},
        }};
        auto cube = vulcao::Image::create(context.allocator(), vk::ImageCreateInfo{
            .flags = vk::ImageCreateFlagBits::eCubeCompatible,
            .imageType = vk::ImageType::e2D,
            .format = sample::color_format,
            .extent = {side, side, 1},
            .mipLevels = 1,
            .arrayLayers = 6,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        });
        // The cube-compatible flag makes Image::create choose a cube view.
        std::vector<sample::Pixel> texels(6 * side * side);
        for (uint32_t face = 0; face < 6; ++face)
            for (uint32_t i = 0; i < side * side; ++i)
                texels[face * side * side + i] = colors[face];
        auto staging = vulcao::Buffer::create(context.allocator(), texels.size() * sizeof(sample::Pixel),
            vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        staging.write(texels);
        auto storage = vulcao::Buffer::create(context.allocator(), 6 * sizeof(std::array<float, 4>),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
        const auto sampler = vulcao::Sampler::nearest(context.device());
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "faces.comp.spv");
        const auto layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&shader.reflection(), 1));
        const auto pipeline = vulcao::Pipeline::create_compute(context.device(), layout, shader, "compMain");
        const std::array sizes{
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1},
        };
        auto pool = vulcao::DescriptorPool::create(context.device(), sizes, 2);
        const auto output_set = pool.allocate(layout.set_layout(0));
        const auto environment_set = pool.allocate(layout.set_layout(1));
        output_set.write_storage_buffer(0, storage);
        vulcao::DescriptorSetWriter(environment_set).write_sampled_image(0, cube)
                                                   .write_sampler(1, sampler).flush();
        const std::array sets{output_set.handle(), environment_set.handle()};
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.transition(cube, vk::ImageLayout::eTransferDstOptimal);
            // One tightly packed copy uploads all six faces, in array-layer order.
            cmd.copy_buffer_to_image(staging.handle(), cube.handle(), vk::BufferImageCopy{
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 6},
                .imageExtent = {side, side, 1},
            });
            cmd.transition(cube, vk::ImageLayout::eShaderReadOnlyOptimal);
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(), sets);
            cmd.dispatch(1);
            cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
        });
        std::array<std::array<float, 4>, 6> result{};
        context.download(storage, result);
        for (size_t face = 0; face < colors.size(); ++face)
            for (size_t channel = 0; channel < 4; ++channel)
                sample::require(std::abs(result[face][channel] - colors[face][channel] / 255.0f) < 0.00001f,
                                "cubemap lookup mismatch for face " + std::to_string(face));
        std::cout << "Verified six cube directions and binding two reflected descriptor sets together\n";
    });
}
