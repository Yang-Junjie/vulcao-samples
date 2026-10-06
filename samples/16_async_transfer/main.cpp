#include <numeric>
#include <string_view>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/fence.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"

int main(int argc, char** argv) {
    return sample::run("16_async_transfer", [&] {
        const bool dedicated = argc == 2 && std::string_view(argv[1]) == "--dedicated";
        sample::require(argc == 1 || dedicated, "usage: async_transfer [--dedicated]");
        vulcao::Context context{{
            .app_name = "16_async_transfer",
            .validation = true,
            .headless = true,
            .device_features = {.timeline_semaphore = true},
            .separate_transfer_queue = dedicated,
        }};
        if (dedicated) {
            bool available = false;
            for (auto device : context.instance().enumeratePhysicalDevices())
                for (const auto& family : device.getQueueFamilyProperties())
                    if (family.queueCount > 0 && (family.queueFlags & vk::QueueFlagBits::eTransfer) &&
                        !(family.queueFlags & vk::QueueFlagBits::eGraphics))
                        available = true;
            if (!available)
                throw sample::Unsupported("no separate transfer queue family is available");
        }
        context.initialize();
        constexpr uint32_t count = 257;
        constexpr vk::Extent2D extent{7, 5};
        const auto families = context.transfer_sharing_families();
        // Concurrent sharing avoids ownership transfers between the two queues.
        // With only one family the factories use exclusive sharing automatically.
        auto storage = vulcao::Buffer::create(context.allocator(), count * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_AUTO, 0, families);
        auto texture = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eSampled, 1, vk::SampleCountFlagBits::e1, families);
        auto readback = vulcao::Buffer::create(context.allocator(), storage.size(),
            vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "consume.comp.spv");
        const auto layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&shader.reflection(), 1));
        const auto pipeline = vulcao::Pipeline::create_compute(context.device(), layout, shader, "compMain");
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), shader.reflection().bindings_for_set(0), 1);
        const auto set = pool.allocate(layout.set_layout(0));
        vulcao::DescriptorSetWriter(set).write_storage_buffer(0, storage)
                                       .write_sampled_image(1, texture).flush();
        auto cmd = vulcao::CommandBuffer::allocate(context.device(), context.command_pool());
        auto fence = vulcao::Fence::create(context.device());
        std::vector<uint32_t> values(count);
        std::iota(values.begin(), values.end(), 0u);
        std::vector<sample::Pixel> texels(extent.width * extent.height, sample::Pixel{7, 31, 127, 255});
        try {
            const auto buffer_upload = context.upload_async(storage, values);
            const auto image_upload = context.upload_async(texture, texels);
            sample::require(image_upload.value > buffer_upload.value, "upload timeline did not advance");
            // upload_async owns a private staging copy; the caller's source can go away.
            values.clear();
            texels.clear();
            cmd.begin();
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(), set.handle());
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eCompute, 0, count);
            cmd.dispatch((count + 63) / 64);
            cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
            cmd.copy_buffer(storage.handle(), readback.handle(), storage.size());
            cmd.buffer_barrier(readback.handle(), vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eHost,
                vk::AccessFlagBits2::eHostRead);
            cmd.end();

            // The GPU waits for both uploads. The CPU only waits at final readback.
            context.submit(context.graphics_queue(), cmd.handle(), context.transfer_timeline(),
                           image_upload.value, vk::PipelineStageFlagBits::eComputeShader, fence.handle());
            fence.wait();
            sample::require(context.transfer_timeline().value() >= image_upload.value,
                            "consumer completed before its upload timeline value");
        } catch (...) {
            // Pending uploads must finish before storage, texture or cmd are destroyed.
            context.wait_idle();
            throw;
        }
        readback.invalidate();
        const auto* result = static_cast<const uint32_t*>(readback.map());
        for (uint32_t i = 0; i < count; ++i)
            sample::require(result[i] == i * 3 + 7, "asynchronous upload/consume mismatch at " + std::to_string(i));
        const auto pixels = sample::read_pixels(context, texture);
        for (uint32_t y = 0; y < extent.height; ++y)
            for (uint32_t x = 0; x < extent.width; ++x)
                sample::expect_pixel(pixels, extent.width, x, y, {7, 31, 127, 255}, 0);
        std::cout << "Verified buffer and image uploads using "
                  << (dedicated ? "a separate transfer queue" : "the graphics queue") << "\n";
    });
}
