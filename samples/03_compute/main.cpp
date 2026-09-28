#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "vulcao/buffer.h"
#include "vulcao/command_buffer.h"
#include "vulcao/context.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/log.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/shader_module.h"

#ifndef VULCAO_SHADER_DIR
#define VULCAO_SHADER_DIR "shaders"
#endif

namespace {

constexpr uint32_t element_count = 64;
constexpr uint32_t workgroup_size = 64;

/// Counted by the log callback so a validation error fails the sample, which
/// keeps the result honest rather than only checking the numbers.
uint32_t validation_errors = 0;

/// Mirrors the transform in transform.slang.
constexpr uint32_t transform(uint32_t value) {
    return value * 2u + 1u;
}

}

int main() {
    vulcao::set_log_level(vulcao::LogLevel::info);
    vulcao::set_log_callback([](const vulcao::LogMessage& message) {
        if (message.category == vulcao::LogCategory::validation &&
            message.level == vulcao::LogLevel::error)
            ++validation_errors;

        std::cout << '[' << vulcao::to_string(message.level) << ": "
                  << vulcao::to_string(message.category) << "] " << message.message << std::endl;
    });

    try {
        // A compute pass needs no surface, so the context is created headless and
        // this sample runs on a machine without a display.
        vulcao::Context context{{.app_name = "03_compute", .headless = true}};
        context.initialize();

        const vulcao::ShaderModule shader = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eCompute,
            std::filesystem::path(VULCAO_SHADER_DIR) / "transform.comp.spv");

        // Both layouts come from the reflection of the shader, so neither the
        // bindings nor the push constant ranges have to be written out by hand.
        const vulcao::DescriptorSetLayout set_layout =
            vulcao::DescriptorSetLayout::create(context.device(), shader.reflection(), 0);
        const vulcao::PipelineLayout pipeline_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&shader.reflection(), 1));
        const vulcao::Pipeline pipeline =
            vulcao::Pipeline::create_compute(context.device(), pipeline_layout, shader, "compMain");

        vulcao::DescriptorPool pool = vulcao::DescriptorPool::create(
            context.device(), vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1}, 1);

        std::vector<uint32_t> values(element_count);
        for (uint32_t i = 0; i < element_count; ++i)
            values[i] = i;

        const vk::DeviceSize bytes = static_cast<vk::DeviceSize>(values.size()) * sizeof(uint32_t);

        // TransferSrc is what lets the dispatch result be copied back out.
        vulcao::Buffer storage = vulcao::Buffer::create_with_data(
            context, values,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);

        const vulcao::DescriptorSet set = pool.allocate(set_layout);
        set.write_storage_buffer(0, storage);

        // The dispatch and the copy back share one submission, so the dependency
        // between them is a single barrier. The readback buffer is host visible,
        // so its contents can be checked without a second submission.
        vulcao::Buffer readback = vulcao::Buffer::create(
            context.allocator(), bytes, vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            // The upload was submitted on its own, so make its transfer write
            // visible to the shader before the shader reads the buffer.
            cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eTransfer,
                               vk::AccessFlagBits2::eTransferWrite,
                               vk::PipelineStageFlagBits2::eComputeShader,
                               vk::AccessFlagBits2::eShaderStorageRead);

            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, pipeline_layout.handle(),
                                     set.handle());
            cmd.dispatch(element_count / workgroup_size);

            // Same for the shader writes on the way out.
            cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                               vk::AccessFlagBits2::eShaderStorageWrite,
                               vk::PipelineStageFlagBits2::eTransfer,
                               vk::AccessFlagBits2::eTransferRead);

            cmd.copy_buffer(storage.handle(), readback.handle(), bytes);
        });

        readback.invalidate();
        const auto* result = static_cast<const uint32_t*>(readback.map());

        for (uint32_t i = 0; i < element_count; ++i) {
            if (result[i] != transform(values[i])) {
                std::cerr << "mismatch at index " << i << ": expected " << transform(values[i])
                          << ", got " << result[i] << std::endl;
                return 1;
            }
        }

        if (validation_errors != 0) {
            std::cerr << validation_errors << " validation error(s) were reported" << std::endl;
            return 1;
        }

        std::cout << "dispatched " << element_count / workgroup_size << " workgroup of "
                  << workgroup_size << " threads over " << element_count
                  << " elements, all results verified" << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
