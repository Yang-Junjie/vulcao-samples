#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>

#include "common/window.h"
#include "vulcao/buffer.h"
#include "vulcao/command_buffer.h"
#include "vulcao/context.h"
#include "vulcao/descriptor_set.h"
#include "vulcao/frame_manager.h"
#include "vulcao/image.h"
#include "vulcao/log.h"
#include "vulcao/pipeline.h"
#include "vulcao/pipeline_layout.h"
#include "vulcao/rendering.h"
#include "vulcao/sampler.h"
#include "vulcao/shader_module.h"
#include "vulcao/vertex_layout.h"

#define STB_IMAGE_IMPLEMENTATION
#include "common/stb_image.h"

#ifndef VULCAO_SHADER_DIR
#define VULCAO_SHADER_DIR "shaders"
#endif

#ifndef VULCAO_ASSET_DIR
#define VULCAO_ASSET_DIR "assets"
#endif

namespace {

struct Vertex {
    float position[2];
    float uv[2];
};

/// A quad spanning the whole render target, with UVs spanning the whole texture.
///
/// The shader takes no uniform buffer, only a push constant that scales the
/// quad, so the pipeline layout reflects a single push constant range. That is
/// the point of this sample: a texture plus the smallest possible amount of
/// per-draw state.
constexpr std::array<Vertex, 4> quad{{
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    {{1.0f, -1.0f}, {1.0f, 0.0f}},
    {{1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f}, {0.0f, 1.0f}},
}};

constexpr std::array<uint16_t, 6> quad_indices{{0, 1, 2, 0, 2, 3}};

struct PushData {
    float scale[2];
    float uv_scale[2];
};

/// One decoded image, owning its pixels.
struct Image2D {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
};

/// Decodes an image file into tightly packed RGBA8.
///
/// stb_image is not part of vulcao; it lives in samples/common to have
/// something to upload. Pass 4 as the desired channel count and it always
/// returns four channels per pixel whatever the file holds, so the upload can
/// assume a single layout. It also does no colour management: the bytes come
/// back in the file's own encoding, which for a PNG is sRGB. See the texture
/// creation below for why that matters.
Image2D load_image(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (data == nullptr)
        throw std::runtime_error("load image: " + path.string() + ": " + stbi_failure_reason());

    Image2D image;
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    image.pixels.assign(data, data + static_cast<size_t>(width) * height * 4);
    stbi_image_free(data);
    return image;
}

}

int main(int argc, char** argv) {
    vulcao::set_log_level(vulcao::LogLevel::info);
    vulcao::set_log_callback([](const vulcao::LogMessage& message) {
        std::cout << '[' << vulcao::to_string(message.level) << ": "
                  << vulcao::to_string(message.category) << "] " << message.message << std::endl;
    });

    try {
        sample::Window window{900, 900, "05_texture"};

        // The swapchain comes from initialize(), so unlike the headless samples
        // this one has an extent to render into and a present queue to hand
        // images to. context.swapchain_format() below is whatever the surface
        // actually gave us, not necessarily what SwapchainInfo asked for first.
        vulcao::Context context{{.app_name = "05_texture"}};
        context.initialize(window.create_surface(context.instance()), window.framebuffer_extent());

        // Any image the decoder understands may be shown; checker.png is only
        // the default so that running with no arguments works.
        const std::filesystem::path image_path =
            argc > 1 ? std::filesystem::path(argv[1])
                     : std::filesystem::path(VULCAO_ASSET_DIR) / "checker.png";
        const Image2D source = load_image(image_path);
        std::cout << "loaded " << source.width << "x" << source.height << " image" << std::endl;

        const std::filesystem::path shader_dir = VULCAO_SHADER_DIR;
        const vulcao::ShaderModule vertex = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eVertex, shader_dir / "textured.vert.spv");
        const vulcao::ShaderModule fragment = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eFragment, shader_dir / "textured.frag.spv");

        // The vertex layout is not written by hand: the attribute formats and
        // locations are read out of the vertex shader's reflection, and this
        // only supplies the byte offsets inside Vertex, which reflection cannot
        // know. One offset per reflected attribute, in location order.
        const vulcao::VertexLayout vertex_layout = vulcao::make_vertex_layout<Vertex>(
            vertex.reflection(), {offsetof(Vertex, position), offsetof(Vertex, uv)});

        vulcao::Buffer vertices = vulcao::Buffer::create_with_data(
            context, quad, vk::BufferUsageFlagBits::eVertexBuffer);
        vulcao::Buffer indices = vulcao::Buffer::create_with_data(
            context, quad_indices, vk::BufferUsageFlagBits::eIndexBuffer);

        // The texture, with a mip chain. TransferSrc is needed next to the
        // sampled usage because upload(..., generate_mips=true) blits level 0
        // down the chain afterwards; the mip chain is what keeps the image from
        // aliasing when the window is smaller than the source.
        //
        // The format is _Srgb rather than _Unorm, and that choice is what makes
        // the colours match an image viewer. The decoded bytes are sRGB-encoded,
        // so an _Srgb view makes the texture unit decode them to linear before
        // the shader sees them; the swapchain is an _Srgb format too, so the
        // value is encoded once more on the way out and the round trip is
        // exactly neutral. Uploading the same bytes through an _Unorm view
        // skips the decode, and the render target then encodes a value that was
        // never decoded: the result looks washed out and too bright.
        // Use _Unorm only for data that is genuinely linear (normal maps,
        // roughness, masks); colour textures from PNG/JPEG want _Srgb.
        const uint32_t mip_levels = 11;
        vulcao::Image texture = vulcao::Image::create_2d(
            context.allocator(), vk::Extent2D{source.width, source.height},
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eTransferSrc,
            mip_levels);

        // upload() is synchronous: it stages the bytes in host memory, copies
        // them, builds the mip chain and leaves the image in the layout asked
        // for, all before returning. No image barrier is needed here afterwards.
        context.upload(texture, source.pixels, vk::ImageLayout::eShaderReadOnlyOptimal,
                       /*generate_mips=*/true);

        // Linear filtering, address mode repeat: the quad covers the whole
        // window, so the sampler is what scales the source to the framebuffer.
        const vulcao::Sampler sampler = vulcao::Sampler::linear(context.device());

        // The descriptor set layout and the pool are both derived from the
        // fragment shader's reflection, so the binding numbers and types the
        // shader declares are the ones used; there is no second place to keep
        // them in sync with. One set is enough because the set is only written
        // once and the texture never changes.
        const vulcao::DescriptorSetLayout set_layout =
            vulcao::DescriptorSetLayout::create(context.device(), fragment.reflection(), 0);
        vulcao::DescriptorPool pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), fragment.reflection().bindings_for_set(0), 1);

        // The pipeline layout merges both stages, so the vertex push constant
        // range and the fragment descriptor set end up in one layout. Entry
        // points are passed explicitly below because Slang emits the names it
        // was given ("vertMain"/"fragMain") rather than "main".
        const std::array<vulcao::ShaderReflection, 2> reflections{vertex.reflection(),
                                                                 fragment.reflection()};
        const vulcao::PipelineLayout pipeline_layout =
            vulcao::PipelineLayout::create_from_reflection(context.device(), reflections);

        // color_formats must match the render target for the pipeline to be
        // usable with it, so it comes from the swapchain rather than a literal.
        const vulcao::Pipeline pipeline = vulcao::Pipeline::create_graphics(
            context.device(), pipeline_layout,
            vulcao::GraphicsPipelineInfo{
                .vertex_shader = vertex.handle(),
                .fragment_shader = fragment.handle(),
                .vertex_entry = "vertMain",
                .fragment_entry = "fragMain",
                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .vertex_bindings = vertex_layout.bindings,
                .vertex_attributes = vertex_layout.attributes,
                .color_formats = {context.swapchain_format()},
            });

        // write_image defaults to the ShaderReadOnly layout, matching what the
        // upload left the image in.
        const vulcao::DescriptorSet set = pool.allocate(set_layout);
        set.write_image(0, texture, sampler);

        vulcao::FrameManager frames{context};

        // begin_frame acquires an image and hands back the slot's command
        // buffer, already reset. The swapchain images handed out by the context
        // are raw handles with no Image wrapper, so they are transitioned
        // through transition_to_render/transition_to_present instead; those two
        // also line the barrier up with the acquire, which is why they must be
        // used rather than a hand-written transition.
        auto render = [&]() {
            vulcao::Frame frame = frames.begin_frame();
            vulcao::CommandBuffer& cmd = *frame.command_buffer;

            const vk::Image image = context.swapchain_images()[frame.image_index];
            const vk::ImageView view = context.swapchain_image_views()[frame.image_index];
            const vk::Extent2D extent = context.swapchain_extent();

            cmd.transition_to_render(image);
            cmd.begin_rendering(
                extent,
                vulcao::color_attachment(view, vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::ClearColorValue{
                                             std::array<float, 4>{0.05f, 0.10f, 0.20f, 1.0f}}));
            cmd.bind_pipeline(pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, pipeline_layout.handle(),
                                     set.handle());
            // Viewport and scissor are dynamic state, so the pipeline needs them
            // set on every recording, after the extent may have changed.
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.push_constants(pipeline_layout.handle(), vk::ShaderStageFlagBits::eVertex, 0,
                               PushData{.scale = {1.0f, 1.0f}, .uv_scale = {1.0f, 1.0f}});
            cmd.bind_vertex_buffer(0, vertices);
            cmd.bind_index_buffer(indices, 0, vk::IndexType::eUint16);
            cmd.draw_indexed(static_cast<uint32_t>(quad_indices.size()));
            cmd.end_rendering();
            cmd.transition_to_present(image);

            frames.end_frame(frame);
            // present() returns false when the image could not be presented and
            // the swapchain has to be rebuilt; it does not throw for that case.
            return frames.present(frame);
        };

        // Zero-size extents happen while a window is minimised, and must not be
        // used to build a swapchain.
        auto recreate_swapchain = [&]() {
            const vk::Extent2D extent = window.framebuffer_extent();
            if (extent.width == 0 || extent.height == 0)
                return false;
            frames.recreate_swapchain(extent);
            return true;
        };

        // Both the resize callback and begin_frame can invalidate the swapchain,
        // so recreating it is handled on both paths. OutOfDateKHRError is
        // expected during normal use (resize, display change) and is not fatal.
        while (!window.should_close()) {
            window.poll_events();

            if (window.consume_resized()) {
                if (!recreate_swapchain())
                    continue;
            }

            try {
                if (!render())
                    recreate_swapchain();
            } catch (const vk::OutOfDateKHRError&) {
                recreate_swapchain();
            }
        }

        // FrameManager waits for in-flight work in its own destructor, but it
        // must be destroyed before the Context; it lives in this scope, so
        // wait_idle here only covers the device-level teardown that follows.
        context.wait_idle();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
