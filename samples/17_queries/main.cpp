#include <array>
#include <limits>

#include "common/sample.h"
#include "vulcao/check.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/query_pool.h"
#include "vulcao/rendering.h"
#include "vulcao/vertex_layout.h"

int main() {
    return sample::run("17_queries", [] {
        vulcao::Context context{{.app_name = "17_queries", .validation = true, .headless = true}};
        context.initialize();
        constexpr vk::Extent2D extent{128, 128};
        const auto properties = context.physical_device().getProperties();
        const auto families = context.physical_device().getQueueFamilyProperties();
        const uint32_t timestamp_bits = families[context.graphics_queue_family_index()].timestampValidBits;
        auto occlusion = vulcao::QueryPool::create(context.device(), vk::QueryType::eOcclusion, 2);
        vulcao::QueryPool timestamps;
        if (timestamp_bits != 0)
            timestamps = vulcao::QueryPool::create(context.device(), vk::QueryType::eTimestamp, 2);
        auto results = vulcao::Buffer::create(context.allocator(), 2 * sizeof(uint64_t),
            vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        auto color = vulcao::Image::create_2d(context.allocator(), extent, sample::color_format,
            vk::ImageUsageFlagBits::eColorAttachment);
        auto depth = vulcao::Image::create_depth(context.allocator(), extent);
        struct Vertex { float position[2]; };
        constexpr std::array<Vertex, 3> triangle{{
            {{0, -0.8f}}, {{0.8f, 0.8f}}, {{-0.8f, 0.8f}},
        }};
        auto vertices = vulcao::Buffer::create_with_data(
            context, triangle, vk::BufferUsageFlagBits::eVertexBuffer);
        const auto vertex = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eVertex,
                                                "query.vert.spv");
        const auto fragment = sample::load_shader(context.device(), vk::ShaderStageFlagBits::eFragment,
                                                  "query.frag.spv");
        const std::array reflections{vertex.reflection(), fragment.reflection()};
        const auto layout = vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);
        const auto input = vulcao::make_vertex_layout<Vertex>(vertex.reflection(), {0});
        const auto pipeline = vulcao::Pipeline::create_graphics(context.device(), layout, {
            .vertex_shader = vertex.handle(),
            .fragment_shader = fragment.handle(),
            .vertex_entry = "vertMain",
            .fragment_entry = "fragMain",
            .depth_test = true,
            .vertex_bindings = input.bindings,
            .vertex_attributes = input.attributes,
            .color_formats = {sample::color_format},
            .depth_format = depth.format(),
        });
        context.immediate([&](vulcao::CommandBuffer& cmd) {
            cmd.reset_query_pool(occlusion.handle(), 0, 2);
            if (timestamps) {
                cmd.reset_query_pool(timestamps.handle(), 0, 2);
                cmd.write_timestamp(timestamps.handle(), vk::PipelineStageFlagBits2::eTopOfPipe, 0);
            }
            cmd.barrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eVertexInput, vk::AccessFlagBits2::eVertexAttributeRead);
            cmd.transition(color, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(depth, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            const auto depth_attachment = vulcao::depth_attachment(depth.view(), depth.layout(), 1.0f);
            cmd.begin_rendering(extent, vulcao::color_attachment(color.view(), color.layout(),
                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}), &depth_attachment);
            cmd.bind_pipeline(pipeline);
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.bind_vertex_buffer(0, vertices);
            // No PRECISE flag: portable occlusion queries only promise zero/nonzero.
            cmd.begin_query(occlusion.handle(), 0);
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eVertex, 0, 0.2f);
            cmd.draw(3);
            cmd.end_query(occlusion.handle(), 0);
            cmd.begin_query(occlusion.handle(), 1);
            cmd.push_constants(layout.handle(), vk::ShaderStageFlagBits::eVertex, 0, 0.8f);
            cmd.draw(3);
            cmd.end_query(occlusion.handle(), 1);
            cmd.end_rendering();
            if (timestamps)
                cmd.write_timestamp(timestamps.handle(), vk::PipelineStageFlagBits2::eBottomOfPipe, 1);
            cmd.copy_query_pool_results(occlusion.handle(), 0, 2, results.handle(), 0, sizeof(uint64_t),
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
            cmd.buffer_barrier(results.handle(), vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eHost,
                vk::AccessFlagBits2::eHostRead);
        });
        results.invalidate();
        const auto* visible = static_cast<const uint64_t*>(results.map());
        sample::require(visible[0] > 0 && visible[1] == 0, "occlusion query did not reject the hidden triangle");
        std::cout << "Visible query = " << visible[0] << ", occluded query = " << visible[1] << '\n';
        if (timestamps) {
            std::array<uint64_t, 2> ticks{};
            vulcao::check(context.device().getQueryPoolResults(timestamps.handle(), 0, 2, sizeof(ticks),
                ticks.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait),
                "read timestamps");
            // Some devices implement fewer than 64 bits; subtraction can wrap.
            const uint64_t mask = timestamp_bits == 64 ? std::numeric_limits<uint64_t>::max()
                                                       : (uint64_t{1} << timestamp_bits) - 1;
            const double microseconds = static_cast<double>((ticks[1] - ticks[0]) & mask) *
                                        properties.limits.timestampPeriod / 1000.0;
            std::cout << "GPU interval: " << microseconds << " us (informational, not a benchmark)\n";
        } else {
            std::cout << "This queue has no timestamps; occlusion queries were verified\n";
        }
    });
}
