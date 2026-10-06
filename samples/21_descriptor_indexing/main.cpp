#include <array>
#include <vector>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"

int main() {
    return sample::run("21_descriptor_indexing", [] {
        // Request only the descriptor-indexing features this shader uses.
        const vk::PhysicalDeviceDescriptorIndexingFeatures required{
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
        };
        vulcao::Context context{{
            .app_name = "21_descriptor_indexing",
            .validation = true,
            .headless = true,
            .customize_selector = [required](vkb::PhysicalDeviceSelector& selector) {
                selector.add_required_extension_features(required);
            },
        }};
        bool supported = false;
        for (auto device : context.instance().enumeratePhysicalDevices()) {
            if (device.getProperties().apiVersion < VK_API_VERSION_1_3)
                continue;
            vk::PhysicalDeviceDescriptorIndexingFeatures indexing;
            vk::PhysicalDeviceFeatures2 features{.pNext = &indexing};
            device.getFeatures2(&features);
            supported |= indexing.shaderSampledImageArrayNonUniformIndexing &&
                         indexing.descriptorBindingSampledImageUpdateAfterBind &&
                         indexing.descriptorBindingPartiallyBound &&
                         indexing.descriptorBindingVariableDescriptorCount && indexing.runtimeDescriptorArray;
        }
        if (!supported)
            throw sample::Unsupported("runtime sampled-image arrays, nonuniform indexing, partial binding, "
                                      "variable counts and update-after-bind are required");
        context.initialize();
        constexpr uint32_t count = 131;
        const auto shader = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                "bindless.comp.spv");
        auto reflected_set = shader.reflection().sets.at(0);
        sample::require(vulcao::set_binding_count(reflected_set, 1, 8),
                        "runtime array binding missing from reflection");
        std::vector<vk::DescriptorBindingFlags> flags;
        for (const auto& binding : reflected_set.bindings)
            flags.push_back(binding.binding == 1
                ? vk::DescriptorBindingFlagBits::ePartiallyBound |
                  vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
                  vk::DescriptorBindingFlagBits::eUpdateAfterBind
                : vk::DescriptorBindingFlags{});
        const auto set_layout = vulcao::DescriptorSetLayout::create(context.device(), reflected_set.bindings,
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, flags);
        const auto layout = vulcao::PipelineLayout::create(
            context.device(), set_layout.handle(), shader.reflection().push_constants);
        const auto pipeline = vulcao::Pipeline::create_compute(context.device(), layout, shader, "compMain");
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), reflected_set.bindings, 1, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
        // Layout capacity is 8; this particular set allocates only 6 descriptors.
        const auto set = pool.allocate(set_layout, 6);
        auto storage = vulcao::Buffer::create(context.allocator(), count * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
        constexpr std::array<uint8_t, 4> red_values{25, 100, 220, 200};
        std::array<vulcao::Image, 4> textures;
        for (size_t i = 0; i < textures.size(); ++i) {
            textures[i] = vulcao::Image::create_2d(context.allocator(), {1, 1}, sample::color_format,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
            context.upload(textures[i], std::array<sample::Pixel, 1>{{{red_values[i], 0, 0, 255}}});
        }
        vulcao::DescriptorSetWriter(set).write_storage_buffer(0, storage)
            .write_sampled_image(1, textures[0], vk::ImageLayout::eShaderReadOnlyOptimal, 0)
            .write_sampled_image(1, textures[1], vk::ImageLayout::eShaderReadOnlyOptimal, 2)
            .write_sampled_image(1, textures[2], vk::ImageLayout::eShaderReadOnlyOptimal, 4).flush();

        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, layout.handle(), set.handle());
            // Legal because the image binding, its layout and pool all opt into
            // UPDATE_AFTER_BIND. No submission is pending during this update.
            vulcao::DescriptorSetWriter(set).write_sampled_image(
                1, textures[3], vk::ImageLayout::eShaderReadOnlyOptimal, 2).flush();
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eCompute, 0, count);
            cmd.dispatch((count + 63) / 64);
            cmd.buffer_barrier(storage.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
        });
        std::vector<uint32_t> result(count);
        context.download(storage, result);
        constexpr std::array<uint32_t, 3> expected{25, 200, 220};
        for (uint32_t i = 0; i < count; ++i)
            sample::require(result[i] == expected[i % 3], "descriptor array lookup mismatch at " + std::to_string(i));
        std::cout << "Verified nonuniform access to slots 0/2/4, variable allocation and update-after-bind\n";
    });
}
