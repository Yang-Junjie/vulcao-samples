#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <syncstream>
#include <utility>
#include <vector>

#include "vulcao/context.h"
#include "vulcao/image.h"
#include "vulcao/log.h"
#include "vulcao/shader_module.h"

#ifndef VULCAO_SHADER_DIR
#define VULCAO_SHADER_DIR "shaders"
#endif

namespace sample {

inline constexpr vk::Format color_format = vk::Format::eR8G8B8A8Unorm;
using Pixel = std::array<uint8_t, 4>;

class Unsupported : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Keep validation capture alive until every sample resource, including the
// context, has been destroyed. The callback can also run on worker threads.
template <typename Fn>
int run(const char* name, Fn&& fn) {
    std::atomic<uint32_t> errors{0};
    vulcao::set_log_level(vulcao::LogLevel::warning);
    vulcao::set_log_callback([&](const vulcao::LogMessage& message) {
        if (message.category == vulcao::LogCategory::validation &&
            message.level == vulcao::LogLevel::error)
            errors.fetch_add(1, std::memory_order_relaxed);
        std::osyncstream(std::cerr) << '[' << vulcao::to_string(message.level) << ": "
                                   << vulcao::to_string(message.category) << "] "
                                   << message.message << '\n';
    });

    int result = 0;
    try {
        std::forward<Fn>(fn)();
    } catch (const Unsupported& error) {
        std::cerr << name << ": SKIP: " << error.what() << '\n';
        result = 77;
    } catch (const std::exception& error) {
        std::cerr << name << ": " << error.what() << '\n';
        result = 1;
    }
    vulcao::set_log_callback({});
    if (errors.load() != 0) {
        std::cerr << errors.load() << " validation error(s)\n";
        result = 1;
    }
    if (result == 0)
        std::cout << name << ": PASS\n";
    return result;
}

inline vulcao::ShaderModule load_shader(vk::Device device, vk::ShaderStageFlagBits stage,
                                         const char* name) {
    return vulcao::ShaderModule::create_from_file(
        device, stage, std::filesystem::path(VULCAO_SHADER_DIR) / name);
}

inline std::vector<uint8_t> read_pixels(vulcao::Context& context, vulcao::Image& image) {
    require(image.format() == color_format, "read_pixels expects an RGBA8 UNORM image");
    std::vector<uint8_t> pixels(static_cast<size_t>(image.extent().width) *
                                image.extent().height * 4);
    context.download(image, pixels);
    return pixels;
}

inline Pixel pixel_at(std::span<const uint8_t> pixels, uint32_t width, uint32_t x, uint32_t y) {
    const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
    require(x < width && offset + 4 <= pixels.size(), "pixel coordinate out of range");
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

inline void expect_pixel(std::span<const uint8_t> pixels, uint32_t width,
                          uint32_t x, uint32_t y, Pixel expected, int tolerance = 1) {
    const Pixel actual = pixel_at(pixels, width, x, y);
    for (size_t channel = 0; channel < 4; ++channel) {
        const int difference = static_cast<int>(actual[channel]) - expected[channel];
        require(difference >= -tolerance && difference <= tolerance,
                "pixel (" + std::to_string(x) + ", " + std::to_string(y) + "), channel " +
                    std::to_string(channel) + ": expected " + std::to_string(expected[channel]) +
                    ", got " + std::to_string(actual[channel]));
    }
}

}
