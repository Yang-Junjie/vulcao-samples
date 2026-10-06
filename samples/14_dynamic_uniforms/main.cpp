#include <array>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("14_dynamic_uniforms", [] {
        vulcao::Context context{{.app_name = "14_dynamic_uniforms", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{96, 32};
        struct ObjectData { std::array<float, 4> color; std::array<float, 4> transform; };
        constexpr std::array<ObjectData, 3> objects{{
            {{1, 0, 0, 1}, {-2.0f / 3.0f, 0, 1.0f / 3.0f, 1}},
            {{0, 1, 0, 1}, {0, 0, 1.0f / 3.0f, 1}},
            {{0, 0, 1, 1}, {2.0f / 3.0f, 0, 1.0f / 3.0f, 1}},
        }};
        const auto alignment = context.physical_device().getProperties().limits.minUniformBufferOffsetAlignment;
        const vk::DeviceSize stride = (sizeof(ObjectData) + alignment - 1) / alignment * alignment;
        auto uniforms = vulcao::Buffer::create(context.allocator(), stride * objects.size(),
            vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        for (size_t i = 0; i < objects.size(); ++i)
            uniforms.write_bytes(&objects[i], sizeof(ObjectData), i * stride);

        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "objects.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "objects.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        auto bindings = vulcao::merge_reflections(reflections).sets.at(0).bindings;
        // SPIR-V describes a uniform buffer. Dynamic offsets are a host-side choice.
        bindings.at(0).descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        const auto set_layout = vulcao::DescriptorSetLayout::create(context.device(), bindings);
        const auto layout = vulcao::PipelineLayout::create(context.device(), set_layout.handle());
        auto pool = vulcao::DescriptorPool::create_for_bindings(context.device(), bindings, 1);
        const auto set = pool.allocate(set_layout);
        // Range covers one object, not the whole buffer: offset + range must stay in bounds.
        set.write_buffer(0, uniforms, vk::DescriptorType::eUniformBufferDynamic, 0, sizeof(ObjectData));

        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 6> quad{{
            {{-1, -1}}, {{1, -1}}, {{1, 1}}, {{-1, -1}}, {{1, 1}}, {{-1, 1}},
        }};
        auto vertices = vulcao::Buffer::create_with_data(
            context, quad, vk::BufferUsageFlagBits::eVertexBuffer);
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
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.begin_rendering(extent, vulcao::color_attachment(target.view(), target.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}));
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            for (uint32_t i = 0; i < objects.size(); ++i) {
                cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, layout.handle(),
                                         set.handle(), static_cast<uint32_t>(i * stride));
                cmd.draw(6);
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
        std::cout << "Three objects share one descriptor set; aligned uniform stride = " << stride << "\n";
    });
}
