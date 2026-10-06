#include <array>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_cache.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/specialization.h"

int main() {
    return sample::run("18_specialization_cache", [] {
        vulcao::Context context{{.app_name = "18_specialization_cache",
                                 .validation = true, .headless = true}};
        context.initialize();
        constexpr uint32_t count = 257;
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "specialize.comp.spv");
        vulcao::DescriptorSetLayoutCache layouts{context.device()};
        const auto reflections = std::span(&shader.reflection(), 1);
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), layouts, reflections);
        const auto second_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), layouts, reflections);
        sample::require(layout.set_layout(0) == second_layout.set_layout(0),
                        "equivalent reflected descriptor layouts were not reused");
        auto cache = vulcao::PipelineCache::create(context.device());
        auto storage = vulcao::Buffer::create(context.allocator(), count * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), shader.reflection().bindings_for_set(0), 1);
        const auto set = pool.allocate(layout.set_layout(0));
        set.write_storage_buffer(0, storage);

        auto verify = [&](const vulcao::Pipeline& pipeline, uint32_t multiplier, uint32_t addend) {
            context.immediate([&](vulcao::CommandBuffer& cmd) {
                // A previous verification may still be the last reader of this buffer.
                cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderStorageWrite);
                cmd.bind_pipeline(pipeline);
                cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(), set.handle());
                cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eCompute, 0, count);
                cmd.dispatch((count + 63) / 64);
                cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead);
            });
            std::vector<uint32_t> result(count);
            context.download(storage, result);
            for (uint32_t i = 0; i < count; ++i)
                sample::require(result[i] == i * multiplier + addend,
                                "specialization mismatch at index " + std::to_string(i));
        };

        for (const auto parameters : {std::array<uint32_t, 2>{2, 1}, {3, 7}, {0, 11}}) {
            vulcao::SpecializationInfo specialization;
            specialization.map_constant(0, parameters[0]).map_constant(1, parameters[1]);
            const auto pipeline = vulcao::Pipeline::create_compute(
                context.device(), cache, layout, shader, "compMain", &specialization);
            verify(pipeline, parameters[0], parameters[1]);
        }
        // These bytes can be persisted. Reload them on the same device/driver;
        // an application's disk cache should check the Vulkan cache header first.
        const auto bytes = cache.data();
        sample::require(!bytes.empty(), "pipeline cache serialization returned no data");
        auto restored_cache = vulcao::PipelineCache::create(context.device(), bytes.data(), bytes.size());
        vulcao::SpecializationInfo specialization;
        specialization.map_constant(0, uint32_t{3}).map_constant(1, uint32_t{7});
        const auto restored_pipeline = vulcao::Pipeline::create_compute(
            context.device(), restored_cache, second_layout, shader, "compMain", &specialization);
        verify(restored_pipeline, 3, 7);
        std::cout << "Verified three specializations, descriptor layout reuse and a "
                  << bytes.size() << "-byte pipeline cache round trip\n";
    });
}
