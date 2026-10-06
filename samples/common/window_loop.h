#pragma once

#include <charconv>
#include <cstdint>
#include <string_view>

#include "common/sample.h"
#include "common/window.h"
#include "vulcao/frame_manager.h"

namespace sample {

// An explicit frame limit makes interactive examples usable as smoke tests.
inline uint32_t frame_limit(int argc, char** argv) {
    if (argc == 1)
        return 0;
    require(argc == 3 && std::string_view(argv[1]) == "--frames",
            "usage: sample [--frames positive-count]");
    const std::string_view text = argv[2];
    uint32_t count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    require(error == std::errc{} && end == text.data() + text.size() && count > 0,
            "--frames expects a positive integer");
    return count;
}

template <typename Render>
void run_window(Window& window, vulcao::Context& context, vulcao::FrameManager& frames,
                uint32_t limit, Render&& render) {
    const double start = glfwGetTime();
    uint32_t rendered = 0;
    bool resize = false;
    try {
        while (!window.should_close() && (limit == 0 || rendered < limit)) {
            window.poll_events();
            if (window.should_close() || glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                break;
            resize = window.consume_resized() || resize;
            const auto extent = window.framebuffer_extent();
            if (extent.width == 0 || extent.height == 0) {
                // Keep the resize pending while minimized; do not acquire a zero-sized frame.
                resize = true;
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            if (resize) {
                frames.recreate_swapchain(extent);
                resize = false;
            }
            try {
                auto frame = frames.begin_frame();
                render(frame, glfwGetTime() - start);
                frames.end_frame(frame);
                resize = !frames.present(frame);
                ++rendered;
            } catch (const vk::OutOfDateKHRError&) {
                resize = true;
            }
        }
        frames.wait_idle();
    } catch (...) {
        // Resources in the caller must outlive any successfully submitted frame.
        context.wait_idle();
        throw;
    }
}

}
