#ifndef OVERLAY_H
#define OVERLAY_H 1

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {UP, DOWN};

struct overlay_settings {
    float bg_color[4];
    uint8_t orientation;
    int zoom;
};

// hmmm maybe introduce a struct for this?
void init_overlay(
    GLFWwindow* _win,
    VkDevice logical_device,
    VkPhysicalDevice physical_device,
    VkQueue graphics_queue,
    uint32_t graphics_queue_index,
    VkFramebuffer* framebuffers,
    uint32_t framebuffers_len,
    VkFormat color_format,
    VkFormat depth_format
);

void resize_overlay(uint32_t framebuffer_width, uint32_t framebuffer_height);

// buffer index is the framebuffer index that is to be rendered to
// use the render finished semaphore of the main program so that
// the overlay has the chance to wait for the main programm to finish
// and then render the overlay on top. Will return a Semaphore that
// that the main program can wait vor
VkSemaphore submit_overlay(struct overlay_settings* settings, uint32_t buffer_index, VkSemaphore main_finished_semaphore);

// cleanup
void shutdown_overlay();

#ifdef __cplusplus
}
#endif
#endif