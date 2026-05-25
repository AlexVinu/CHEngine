#pragma once

#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#include <Log/Log.h>
#include <cstdlib>

// Evaluates a VkResult expression; logs and aborts on failure.
// Works in both bool-returning and void functions.
#define VK_CHECK_RESULT(expr, message)                                              \
    do {                                                                            \
        VkResult _vkr = (expr);                                                     \
        if (_vkr != VK_SUCCESS) {                                                   \
            CHE_CORE_ERROR("Vulkan {}: {} ({})", message,                           \
                           string_VkResult(_vkr), static_cast<int>(_vkr));          \
            std::abort();                                                           \
        }                                                                           \
    } while (0)
