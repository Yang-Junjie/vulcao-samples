#include <array>
#include <numeric>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"

int main() {
    return sample::run("12_compute_reduction", [] {
        vulcao::Context context{{.app_name = "12_compute_reduction",
                                 .validation = true, .headless = true}};
        context.initialize();
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "reduce.comp.spv");
        const auto layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&shader.reflection(), 1));
        const auto pipeline = vulcao::Pipeline::create_compute(context.device(), layout, shader, "compMain");

        for (const uint32_t count : {1u, 63u, 64u, 65u, 1003u, 4099u}) {
            std::vector<uint32_t> input(count);
            for (uint32_t i = 0; i < count; ++i)
                input[i] = i % 17 + 1;
            const uint32_t expected = std::accumulate(input.begin(), input.end(), 0u);
            constexpr auto usage = vk::BufferUsageFlagBits::eStorageBuffer |
                                   vk::BufferUsageFlagBits::eTransferSrc;
            auto a = vulcao::Buffer::create_with_data(context, input, usage);
            auto b = vulcao::Buffer::create(context.allocator(), a.size(), usage);
            auto pool = vulcao::DescriptorPool::create_for_bindings(
                context.device(), shader.reflection().bindings_for_set(0), 2);
            const std::array sets{pool.allocate(layout.set_layout(0)), pool.allocate(layout.set_layout(0))};
            vulcao::DescriptorSetWriter(sets[0]).write_storage_buffer(0, a).write_storage_buffer(1, b).flush();
            vulcao::DescriptorSetWriter(sets[1]).write_storage_buffer(0, b).write_storage_buffer(1, a).flush();
            vulcao::Buffer* result = nullptr;
            uint32_t passes = 0;

            context.immediate([&](vulcao::CommandBuffer& cmd) {
                cmd.buffer_barrier(a.handle(), vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderStorageRead);
                cmd.bind_pipeline(pipeline);
                uint32_t remaining = count;
                uint32_t direction = 0;
                do {
                    const uint32_t groups = (remaining + 63) / 64;
                    cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(),
                                             sets[direction].handle());
                    cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eCompute, 0, remaining);
                    cmd.dispatch(groups);
                    result = direction == 0 ? &b : &a;
                    ++passes;
                    remaining = groups;
                    direction ^= 1;
                    if (remaining > 1) {
                        // Ping-pong reuse needs both the write->read dependency and
                        // ordering of earlier reads before overwriting the old input.
                        cmd.barrier(vk::PipelineStageFlagBits2::eComputeShader,
                            vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
                            vk::PipelineStageFlagBits2::eComputeShader,
                            vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);
                    }
                } while (remaining > 1);
                cmd.buffer_barrier(result->handle(), vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead);
            });
            uint32_t sum = 0;
            context.download(*result, &sum, sizeof(sum));
            sample::require(sum == expected, "incorrect reduction for " + std::to_string(count) + " elements");
            std::cout << count << " elements -> " << sum << " in " << passes << " pass(es)\n";
        }
    });
}
