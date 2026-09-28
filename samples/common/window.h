#pragma once

#include <cstdint>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

namespace sample {

/// @brief RAII GLFW window that owns the GLFW library lifetime.
class Window {
public:
    Window(uint32_t width, uint32_t height, const char* title) {
        if (!glfwInit())
            throw std::runtime_error("glfwInit failed");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr,
                                   nullptr);
        if (window_ == nullptr) {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int, int) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->resized_ = true;
        });
    }

    ~Window() {
        if (window_ != nullptr)
            glfwDestroyWindow(window_);
        glfwTerminate();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* handle() const { return window_; }

    bool should_close() const { return glfwWindowShouldClose(window_) != 0; }

    void poll_events() const { glfwPollEvents(); }

    /// @brief Returns true once if the framebuffer was resized since the last call.
    bool consume_resized() {
        const bool resized = resized_;
        resized_ = false;
        return resized;
    }

    vk::Extent2D framebuffer_extent() const {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    vk::SurfaceKHR create_surface(vk::Instance instance) const {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("glfwCreateWindowSurface failed");
        return vk::SurfaceKHR{surface};
    }

private:
    GLFWwindow* window_ = nullptr;
    bool resized_ = false;
};

}
