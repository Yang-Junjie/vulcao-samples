#include <array>

#include "common/sample.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("13_indirect_draw", [] {
        vulcao::Context context{{.app_name = "13_indirect_draw", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{96, 96};
        struct Vertex { float position[2]; };
        auto vertices = vulcao::Buffer::create(context.allocator(), 3 * sizeof(Vertex),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer);
        auto arguments = vulcao::Buffer::create(context.allocator(), sizeof(vk::DrawIndirectCommand),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eTransferSrc);
        constexpr std::array<uint32_t, 3> dispatch_size{1, 1, 1};
        auto dispatch_arguments = vulcao::Buffer::create_with_data(
            context, dispatch_size, vk::BufferUsageFlagBits::eIndirectBuffer);
        const auto compute = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eCompute,
                                                 "generate.comp.spv");
        const auto compute_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), std::span(&compute.reflection(), 1));
        const auto compute_pipeline = vulcao::Pipeline::create_compute(
            context.device(), compute_layout, compute, "compMain");
        auto pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), compute.reflection().bindings_for_set(0), 1);
        const auto set = pool.allocate(compute_layout.set_layout(0));
        vulcao::DescriptorSetWriter(set).write_storage_buffer(0, vertices)
                                       .write_storage_buffer(1, arguments).flush();

        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "draw.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "draw.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto graphics_layout = vulcao::PipelineLayout::create_from_reflection(
            context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        const auto graphics_pipeline = vulcao::Pipeline::create_graphics(context.device(), graphics_layout, {
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
            cmd.buffer_barrier(dispatch_arguments.handle(), vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eDrawIndirect,
                vk::AccessFlagBits2::eIndirectCommandRead);
            cmd.bind_pipeline(compute_pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, compute_layout.handle(), set.handle());
            cmd.dispatch_indirect(dispatch_arguments.handle(), 0);

            // Generated vertices and arguments have different consumers.
            cmd.buffer_barrier(vertices.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eVertexInput,
                vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.barrier(vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite, vk::PipelineStageFlagBits2::eDrawIndirect,
                vk::AccessFlagBits2::eIndirectCommandRead);
            cmd.transition(target, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.begin_rendering(extent, vulcao::color_attachment(target.view(), target.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}));
            cmd.bind_pipeline(graphics_pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            // One command needs neither multiDrawIndirect nor a nonzero firstInstance.
            cmd.draw_indirect(arguments.handle(), 0, 1, sizeof(vk::DrawIndirectCommand));
            cmd.end_rendering();
            cmd.buffer_barrier(arguments.handle(), vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
        });
        std::array<uint32_t, 4> result{};
        context.download(arguments, result);
        sample::require(result == std::array<uint32_t, 4>{3, 1, 0, 0},
                        "GPU-generated draw arguments are incorrect: " + std::to_string(result[0]) +
                        ", " + std::to_string(result[1]) + ", " + std::to_string(result[2]) +
                        ", " + std::to_string(result[3]));
        const auto pixels = sample::read_pixels(context, target);
        sample::expect_pixel(pixels, extent.width, 48, 48, {255, 128, 0, 255});
        sample::expect_pixel(pixels, extent.width, 0, 0, {0, 0, 0, 255});
        std::cout << "Verified indirect dispatch -> generated geometry/arguments -> indirect draw\n";
    });
}
