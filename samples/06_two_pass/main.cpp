#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

#define TINYOBJLOADER_IMPLEMENTATION
#include "common/tiny_obj_loader.h"

#ifndef VULCAO_SHADER_DIR
#define VULCAO_SHADER_DIR "shaders"
#endif

#ifndef VULCAO_ASSET_DIR
#define VULCAO_ASSET_DIR "assets"
#endif

namespace {

/// The offscreen pass and the post pass share the swapchain extent, so the
/// scene is shaded at the window's resolution.
constexpr vk::Format scene_color_format = vk::Format::eR16G16B16A16Sfloat;
constexpr vk::Format scene_depth_format = vk::Format::eD32Sfloat;

constexpr std::array<float, 4> clear_color{0.02f, 0.03f, 0.05f, 1.0f};
constexpr std::array<float, 4> teapot_color{0.85f, 0.45f, 0.20f, 1.0f};
/// x ambient, y diffuse, z specular, w shininess.
constexpr std::array<float, 4> teapot_material{0.06f, 0.85f, 0.80f, 96.0f};

struct Vertex {
    float position[3];
    float normal[3];
};

struct FullscreenVertex {
    float position[2];
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/// Matches SceneUniforms in scene.slang.
struct SceneUniforms {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 camera_position;
    glm::vec4 light_direction;
    glm::vec4 light_color;
};

/// Matches PushData in scene.slang.
struct PushData {
    glm::mat4 model;
    glm::vec4 base_color;
    glm::vec4 material;
};

/// Matches PostUniforms in post.slang.
struct PostPushData {
    float exposure;
    float gamma;
    float padding[2];
};

/// Loads the teapot, recomputes smooth normals (the asset ships none) and
/// normalizes the model to roughly unit size around the origin.
Mesh load_teapot(const std::filesystem::path& path) {
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path.string()))
        throw std::runtime_error("failed to load " + path.string() + ": " + reader.Error());

    const tinyobj::attrib_t& attrib = reader.GetAttrib();

    Mesh mesh;
    mesh.vertices.resize(attrib.vertices.size() / 3);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i].position[0] = attrib.vertices[i * 3 + 0];
        mesh.vertices[i].position[1] = attrib.vertices[i * 3 + 1];
        mesh.vertices[i].position[2] = attrib.vertices[i * 3 + 2];
        mesh.vertices[i].normal[0] = 0.0f;
        mesh.vertices[i].normal[1] = 0.0f;
        mesh.vertices[i].normal[2] = 0.0f;
    }

    for (const tinyobj::shape_t& shape : reader.GetShapes())
        for (const tinyobj::index_t& index : shape.mesh.indices)
            mesh.indices.push_back(static_cast<uint32_t>(index.vertex_index));

    // Area weighted smooth normals: the cross product magnitude is the face area.
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[t + 0]];
        const Vertex& b = mesh.vertices[mesh.indices[t + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[t + 2]];

        const glm::vec3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                           b.position[2] - a.position[2]};
        const glm::vec3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                           c.position[2] - a.position[2]};
        const glm::vec3 n = glm::cross(ab, ac);

        for (size_t k = 0; k < 3; ++k) {
            Vertex& v = mesh.vertices[mesh.indices[t + k]];
            v.normal[0] += n.x;
            v.normal[1] += n.y;
            v.normal[2] += n.z;
        }
    }

    glm::vec3 box_min{1e30f};
    glm::vec3 box_max{-1e30f};
    for (Vertex& vertex : mesh.vertices) {
        const float length = std::sqrt(vertex.normal[0] * vertex.normal[0] +
                                       vertex.normal[1] * vertex.normal[1] +
                                       vertex.normal[2] * vertex.normal[2]);
        if (length > 1e-6f) {
            vertex.normal[0] /= length;
            vertex.normal[1] /= length;
            vertex.normal[2] /= length;
        }

        box_min = glm::min(box_min, glm::vec3{vertex.position[0], vertex.position[1],
                                              vertex.position[2]});
        box_max = glm::max(box_max, glm::vec3{vertex.position[0], vertex.position[1],
                                              vertex.position[2]});
    }

    const glm::vec3 center = (box_min + box_max) * 0.5f;
    const float scale = 1.6f / glm::max(glm::max(box_max.x - box_min.x, box_max.y - box_min.y),
                                        box_max.z - box_min.z);
    for (Vertex& vertex : mesh.vertices)
        for (size_t i = 0; i < 3; ++i)
            vertex.position[i] = (vertex.position[i] - center[static_cast<int>(i)]) * scale;

    return mesh;
}

}

int main() {
    vulcao::set_log_level(vulcao::LogLevel::info);
    vulcao::set_log_callback([](const vulcao::LogMessage& message) {
        std::cout << '[' << vulcao::to_string(message.level) << ": "
                  << vulcao::to_string(message.category) << "] " << message.message << std::endl;
    });

    try {
        sample::Window window{900, 900, "06_two_pass"};

        // The post pass applies its own gamma, so the swapchain is explicitly a
        // UNORM format. An sRGB swapchain would encode the final value in
        // hardware as well and the result would be gamma corrected twice.
        const vk::SurfaceFormatKHR surface_format{
            .format = vk::Format::eR8G8B8A8Unorm,
            .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear};

        vulcao::Context context{{.app_name = "06_two_pass"}};
        context.initialize(window.create_surface(context.instance()), window.framebuffer_extent(),
                           vulcao::SwapchainInfo{.formats = {surface_format}});

        const std::filesystem::path shader_dir = VULCAO_SHADER_DIR;
        const Mesh mesh = load_teapot(std::filesystem::path{VULCAO_ASSET_DIR} / "teapot.obj");

        vulcao::Buffer vertex_buffer =
            vulcao::Buffer::create_with_data(context, mesh.vertices,
                                             vk::BufferUsageFlagBits::eVertexBuffer);
        vulcao::Buffer index_buffer =
            vulcao::Buffer::create_with_data(context, mesh.indices,
                                             vk::BufferUsageFlagBits::eIndexBuffer);

        // The fullscreen triangle of the post pass. A vertex buffer rather than
        // SV_VertexID keeps the device requirements at their minimum.
        constexpr std::array<FullscreenVertex, 3> fullscreen{{
            {{-1.0f, -1.0f}},
            {{3.0f, -1.0f}},
            {{-1.0f, 3.0f}},
        }};
        vulcao::Buffer fullscreen_buffer = vulcao::Buffer::create_with_data(
            context, fullscreen, vk::BufferUsageFlagBits::eVertexBuffer);

        const vulcao::ShaderModule scene_vertex = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eVertex, shader_dir / "scene.vert.spv");
        const vulcao::ShaderModule scene_fragment = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eFragment, shader_dir / "scene.frag.spv");
        const vulcao::ShaderModule post_vertex = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eVertex, shader_dir / "post.vert.spv");
        const vulcao::ShaderModule post_fragment = vulcao::ShaderModule::create_from_file(
            context.device(), vk::ShaderStageFlagBits::eFragment, shader_dir / "post.frag.spv");

        // Pass 1 renders into these; pass 2 samples the color one and writes the
        // tonemapped result straight into the acquired swapchain image. The
        // color target is a floating point format so the lit values may exceed 1
        // and the tonemapper has something to roll off. Both are recreated at
        // the swapchain extent whenever the window is resized.
        vulcao::Image scene_color;
        vulcao::Image scene_depth;
        auto create_scene_targets = [&]() {
            const vk::Extent2D extent = context.swapchain_extent();
            scene_color = vulcao::Image::create_2d(
                context.allocator(), extent, scene_color_format,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
            scene_depth = vulcao::Image::create_depth(context.allocator(), extent,
                                                      scene_depth_format);
        };
        create_scene_targets();

        // The scene pipeline layout merges both stages' reflections, so the
        // uniform buffer and the push constant block shared by the vertex and
        // fragment stages line up.
        const std::array<vulcao::ShaderReflection, 2> scene_reflections{scene_vertex.reflection(),
                                                                        scene_fragment.reflection()};
        const vulcao::PipelineLayout scene_layout =
            vulcao::PipelineLayout::create_from_reflection(context.device(), scene_reflections);

        const vulcao::VertexLayout scene_vertex_layout = vulcao::make_vertex_layout<Vertex>(
            scene_vertex.reflection(), {offsetof(Vertex, position), offsetof(Vertex, normal)});

        const vulcao::Pipeline scene_pipeline = vulcao::Pipeline::create_graphics(
            context.device(), scene_layout,
            vulcao::GraphicsPipelineInfo{
                .vertex_shader = scene_vertex.handle(),
                .fragment_shader = scene_fragment.handle(),
                .vertex_entry = "vertMain",
                .fragment_entry = "fragMain",
                .depth_test = true,
                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .vertex_bindings = scene_vertex_layout.bindings,
                .vertex_attributes = scene_vertex_layout.attributes,
                .color_formats = {scene_color_format},
                .depth_format = scene_depth_format,
            });

        // The post pipeline gets its descriptor layout from the fragment
        // reflection, which is also what sizes the descriptor pool. Its color
        // format is the swapchain's, since it renders directly into it.
        const std::array<vulcao::ShaderReflection, 2> post_reflections{post_vertex.reflection(),
                                                                       post_fragment.reflection()};
        const vulcao::PipelineLayout post_layout =
            vulcao::PipelineLayout::create_from_reflection(context.device(), post_reflections);

        const vulcao::VertexLayout post_vertex_layout =
            vulcao::make_vertex_layout<FullscreenVertex>(post_vertex.reflection(), {0});

        const vulcao::Pipeline post_pipeline = vulcao::Pipeline::create_graphics(
            context.device(), post_layout,
            vulcao::GraphicsPipelineInfo{
                .vertex_shader = post_vertex.handle(),
                .fragment_shader = post_fragment.handle(),
                .vertex_entry = "vertMain",
                .fragment_entry = "fragMain",
                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .vertex_bindings = post_vertex_layout.bindings,
                .vertex_attributes = post_vertex_layout.attributes,
                .color_formats = {context.swapchain_format()},
            });

        const vulcao::Sampler sampler = vulcao::Sampler::linear(context.device());
        vulcao::DescriptorPool post_pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), post_fragment.reflection().bindings_for_set(0), 1);
        const vulcao::DescriptorSet scene_set = post_pool.allocate(post_layout.set_layout(0));
        scene_set.write_image(0, scene_color, sampler);

        // One frame in flight: every frame reuses the same offscreen scene
        // image, so the next frame must not start recording its pass 1 until
        // the previous frame's pass 2 has finished sampling it. Waiting on a
        // single slot's fence is what guarantees that, and the tiny offscreen
        // render does not need the extra overlap.
        vulcao::FrameManager frames{context, {.frames_in_flight = 1}};
        const uint32_t frame_count = frames.frames_in_flight();

        // The camera and light never move, so the per-frame uniform buffer is
        // filled once. Only the per-object model matrix animates, and that is a
        // push constant.
        const glm::vec3 camera_position{0.0f, 0.0f, 3.0f};
        const glm::vec3 light_direction = glm::normalize(glm::vec3{0.5f, 1.0f, 0.6f});
        const SceneUniforms scene_uniforms{
            .view = glm::lookAt(camera_position, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}),
            .projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f),
            .camera_position = glm::vec4{camera_position.x, camera_position.y,
                                         camera_position.z, 1.0f},
            .light_direction = glm::vec4{light_direction.x, light_direction.y,
                                         light_direction.z, 0.0f},
            .light_color = glm::vec4{1.0f, 0.97f, 0.92f, 3.0f},
        };

        std::vector<vulcao::Buffer> uniform_buffers;
        uniform_buffers.reserve(frame_count);
        for (uint32_t i = 0; i < frame_count; ++i) {
            vulcao::Buffer buffer = vulcao::Buffer::create(
                context.allocator(), sizeof(SceneUniforms), vk::BufferUsageFlagBits::eUniformBuffer,
                VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
            buffer.write_bytes(&scene_uniforms, sizeof(scene_uniforms));
            uniform_buffers.push_back(std::move(buffer));
        }

        vulcao::DescriptorPool scene_pool = vulcao::DescriptorPool::create_for_bindings(
            context.device(), scene_fragment.reflection().bindings_for_set(0), frame_count);
        std::vector<vulcao::DescriptorSet> scene_sets;
        scene_sets.reserve(frame_count);
        for (uint32_t i = 0; i < frame_count; ++i) {
            vulcao::DescriptorSet set = scene_pool.allocate(scene_layout.set_layout(0));
            set.write_uniform_buffer(0, uniform_buffers[i]);
            scene_sets.push_back(set);
        }

        const PostPushData post_push_data{
            .exposure = 1.0f,
            .gamma = 1.0f / 2.2f,
            .padding = {0.0f, 0.0f},
        };

        const double start_time = glfwGetTime();

        auto render = [&](double time_seconds) {
            vulcao::Frame frame = frames.begin_frame();
            vulcao::CommandBuffer& cmd = *frame.command_buffer;
            const uint32_t slot = frame.slot;

            const vk::Image image = context.swapchain_images()[frame.image_index];
            const vk::ImageView view = context.swapchain_image_views()[frame.image_index];
            const vk::Extent2D extent = context.swapchain_extent();

            // The teapot spins so the specular highlight travels across it.
            const glm::mat4 model =
                glm::rotate(glm::mat4{1.0f}, static_cast<float>(time_seconds) * 0.8f,
                            glm::vec3{0.0f, 1.0f, 0.0f}) *
                glm::rotate(glm::mat4{1.0f}, 0.3f, glm::vec3{1.0f, 0.0f, 0.0f});
            const PushData push_data{
                .model = model,
                .base_color = glm::vec4{teapot_color[0], teapot_color[1], teapot_color[2],
                                        teapot_color[3]},
                .material = glm::vec4{teapot_material[0], teapot_material[1], teapot_material[2],
                                      teapot_material[3]},
            };

            // Pass 1: Blinn-Phong into the offscreen HDR color and depth images.
            cmd.transition(scene_color, vk::ImageLayout::eColorAttachmentOptimal);
            cmd.transition(scene_depth, vk::ImageLayout::eDepthStencilAttachmentOptimal);

            const vk::RenderingAttachmentInfo depth_attachment = vulcao::depth_attachment(
                scene_depth.view(), scene_depth.layout(), 1.0f);
            cmd.begin_rendering(extent,
                                vulcao::color_attachment(scene_color.view(), scene_color.layout(),
                                                         vk::ClearColorValue{clear_color}),
                                &depth_attachment);
            cmd.bind_pipeline(scene_pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, scene_layout.handle(),
                                     scene_sets[slot].handle());
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.push_constants(scene_layout.handle(),
                               vk::ShaderStageFlagBits::eVertex |
                                   vk::ShaderStageFlagBits::eFragment,
                               0, push_data);
            cmd.bind_vertex_buffer(0, vertex_buffer);
            cmd.bind_index_buffer(index_buffer, 0, vk::IndexType::eUint32);
            cmd.draw_indexed(static_cast<uint32_t>(mesh.indices.size()));
            cmd.end_rendering();

            // Pass 2: the color attachment becomes the sampled texture, and the
            // tonemapped post effect is drawn fullscreen into the swapchain.
            cmd.transition(scene_color, vk::ImageLayout::eShaderReadOnlyOptimal);
            cmd.transition_to_render(image);

            cmd.begin_rendering(
                extent,
                vulcao::color_attachment(view, vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::ClearColorValue{
                                             std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}));
            cmd.bind_pipeline(post_pipeline);
            cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, post_layout.handle(),
                                     scene_set.handle());
            cmd.set_viewport(extent);
            cmd.set_scissor(extent);
            cmd.push_constants(post_layout.handle(), vk::ShaderStageFlagBits::eFragment, 0,
                               post_push_data);
            cmd.bind_vertex_buffer(0, fullscreen_buffer);
            cmd.draw(3);
            cmd.end_rendering();

            cmd.transition_to_present(image);

            frames.end_frame(frame);
            return frames.present(frame);
        };

        auto recreate_swapchain = [&]() {
            const vk::Extent2D extent = window.framebuffer_extent();
            if (extent.width == 0 || extent.height == 0)
                return false;
            // recreate_swapchain waits for the device to go idle, so the old
            // offscreen images can be replaced and the descriptor set rewritten.
            frames.recreate_swapchain(extent);
            create_scene_targets();
            scene_set.write_image(0, scene_color, sampler);
            return true;
        };

        while (!window.should_close()) {
            window.poll_events();

            if (window.consume_resized()) {
                if (!recreate_swapchain())
                    continue;
            }

            try {
                if (!render(glfwGetTime() - start_time))
                    recreate_swapchain();
            } catch (const vk::OutOfDateKHRError&) {
                recreate_swapchain();
            }
        }

        context.wait_idle();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
