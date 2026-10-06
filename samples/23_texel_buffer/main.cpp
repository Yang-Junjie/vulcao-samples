#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"

int main() {
    return sample::run("23_texel_buffer", [] {
        vulcao::Context context{{.app_name = "23_texel_buffer", .validation = true, .headless = true}};
        context.initialize();
        constexpr uint32_t count = 97;
        constexpr auto format = vk::Format::eR32Uint;
        const auto features = context.physical_device().getFormatProperties(format).bufferFeatures;
        const auto needed = vk::FormatFeatureFlagBits::eUniformTexelBuffer |
                            vk::FormatFeatureFlagBits::eStorageTexelBuffer;
        if ((features & needed) != needed)
            throw sample::Unsupported("R32Uint uniform and storage texel buffers are unavailable");
        std::vector<uint32_t> input(count);
        for (uint32_t i = 0; i < count; ++i)
            input[i] = i * 17 + 3;
        auto source = vulcao::Buffer::create_with_data(
            context, input, vk::BufferUsageFlagBits::eUniformTexelBuffer);
        auto destination = vulcao::Buffer::create(context.allocator(), source.size(),
            vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eTransferSrc);
        // Buffer views provide the texel format; their buffers must outlive them.
        const auto source_view = vulcao::BufferView::create(context.device(), source, format);
        const auto destination_view = vulcao::BufferView::create(context.device(), destination, format);
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "texels.comp.spv");
        const auto layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&shader.reflection(), 1));
        const auto pipeline = vulcao::Pipeline::create_compute(context.device(), layout, shader, "compMain");
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), shader.reflection().bindings_for_set(0), 1);
        const auto set = pool.allocate(layout.set_layout(0));
        vulcao::DescriptorSetWriter(set).write_uniform_texel_buffer(0, source_view)
                                       .write_storage_texel_buffer(1, destination_view).flush();
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.buffer_barrier(source.handle(), vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderSampledRead);
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(), set.handle());
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eCompute, 0, count);
            cmd.dispatch((count + 63) / 64);
            cmd.buffer_barrier(destination.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
        });
        std::vector<uint32_t> result(count);
        context.download(destination, result);
        for (uint32_t i = 0; i < count; ++i)
            sample::require(result[i] == (input[i] ^ 0x5a5a5a5au),
                            "typed texel buffer mismatch at index " + std::to_string(i));
        std::cout << "Verified R32Uint uniform/storage texel buffer views over " << count << " elements\n";
    });
}
