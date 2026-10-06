#include <algorithm>
#include <array>
#include <vector>

#include "common/sample.h"

int main() {
    return sample::run("20_transfer_regions", [] {
        vulcao::Context context{{.app_name = "20_transfer_regions",
                                 .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{8, 6};
        constexpr uint32_t source_offset = 4; // In pixels; Vulkan's bufferOffset is in bytes.
        constexpr uint32_t source_pitch = 5;
        constexpr uint32_t destination_offset = 8;
        constexpr uint32_t destination_pitch = 11;
        constexpr uint32_t destination_rows = 8;
        const sample::Pixel black{0, 0, 0, 255};
        std::vector<sample::Pixel> source(source_offset + source_pitch * 4, sample::Pixel{19, 29, 39, 49});
        std::vector<sample::Pixel> expected(extent.width * extent.height, black);
        for (uint32_t y = 0; y < 2; ++y)
            for (uint32_t x = 0; x < 3; ++x) {
                const sample::Pixel color{static_cast<uint8_t>(40 + x * 70),
                                          static_cast<uint8_t>(60 + y * 80), 90, 255};
                source[source_offset + y * source_pitch + x] = color;
                expected[(y + 1) * extent.width + x + 2] = color;
            }
        auto staging = vulcao::Buffer::create(context.allocator(), source.size() * sizeof(sample::Pixel),
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
        auto image = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled, 3);
        const vk::DeviceSize readback_bytes =
            (destination_offset + destination_pitch * destination_rows) * sizeof(sample::Pixel);
        auto readback = vulcao::Buffer::create(context.allocator(), readback_bytes,
            vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        // Mip 1 is 4x3; mip 2 is 2x1. Both are packed into one buffer.
        auto mip_readback = vulcao::Buffer::create(context.allocator(), (4 * 3 + 2 * 1) * sizeof(sample::Pixel),
            vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            // update_buffer is convenient for small uploads (at most 64 KiB).
            cmd.update_buffer(staging.handle(), 0, source);
            cmd.fill_buffer(readback.handle(), 0, readback.size(), 0xcdcdcdcdu);
            cmd.transition(image, vk::ImageLayout::eTransferDstOptimal);
            cmd.clear_color_image(image, vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}});
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eTransfer,
                        vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite);
            cmd.copy_buffer_to_image(staging.handle(), image.handle(), vk::BufferImageCopy{
                .bufferOffset = source_offset * sizeof(sample::Pixel),
                .bufferRowLength = source_pitch,
                .bufferImageHeight = 4,
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                .imageOffset = {2, 1, 0},
                .imageExtent = {3, 2, 1},
            });
            cmd.generate_mipmaps(image, vk::ImageLayout::eShaderReadOnlyOptimal, vk::Filter::eNearest);
            cmd.transition(image, vk::ImageLayout::eTransferSrcOptimal);
            cmd.copy_image_to_buffer(readback.handle(), image.handle(), vk::BufferImageCopy{
                .bufferOffset = destination_offset * sizeof(sample::Pixel),
                .bufferRowLength = destination_pitch,
                .bufferImageHeight = destination_rows,
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                .imageExtent = {extent.width, extent.height, 1},
            });
            vk::DeviceSize offset = 0;
            for (uint32_t mip = 1; mip < 3; ++mip) {
                const uint32_t width = std::max(1u, extent.width >> mip);
                const uint32_t height = std::max(1u, extent.height >> mip);
                cmd.copy_image_to_buffer(mip_readback.handle(), image.handle(), vk::BufferImageCopy{
                    .bufferOffset = offset,
                    .imageSubresource = {vk::ImageAspectFlagBits::eColor, mip, 0, 1},
                    .imageExtent = {width, height, 1},
                });
                offset += width * height * sizeof(sample::Pixel);
            }
            cmd.transition(image, vk::ImageLayout::eShaderReadOnlyOptimal);
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
        });
        readback.invalidate();
        const auto* actual = static_cast<const sample::Pixel*>(readback.map());
        std::vector<sample::Pixel> padded(destination_offset + destination_pitch * destination_rows,
                                          sample::Pixel{0xcd, 0xcd, 0xcd, 0xcd});
        for (uint32_t y = 0; y < extent.height; ++y)
            for (uint32_t x = 0; x < extent.width; ++x)
                padded[destination_offset + y * destination_pitch + x] = expected[y * extent.width + x];
        for (size_t i = 0; i < padded.size(); ++i)
            sample::require(actual[i] == padded[i], "row-pitched copy or padding mismatch at texel " + std::to_string(i));

        mip_readback.invalidate();
        const auto* mip_pixels = static_cast<const sample::Pixel*>(mip_readback.map());
        uint32_t previous_width = extent.width, previous_height = extent.height;
        size_t offset = 0;
        for (uint32_t mip = 1; mip < 3; ++mip) {
            const uint32_t width = std::max(1u, previous_width / 2);
            const uint32_t height = std::max(1u, previous_height / 2);
            std::vector<sample::Pixel> next(width * height);
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x) {
                    const uint32_t source_x = (2 * x + 1) * previous_width / (2 * width);
                    const uint32_t source_y = (2 * y + 1) * previous_height / (2 * height);
                    next[y * width + x] = expected[source_y * previous_width + source_x];
                    sample::require(mip_pixels[offset + y * width + x] == next[y * width + x],
                                    "nearest-filtered mip mismatch at level " + std::to_string(mip));
                }
            offset += next.size();
            expected = std::move(next);
            previous_width = width;
            previous_height = height;
        }
        std::cout << "Verified partial image upload, row pitches, untouched padding and two mip levels\n";
    });
}
