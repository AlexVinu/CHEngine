#pragma once

#include <vulkan/vulkan_core.h>

#include <Log/Log.h>

#define VK_CHECK_RESULT(res, message) \
{ \
    static_assert(std::is_same_v<decltype(res), VkResult>, \
                  "VK_CHECK_RESULT: expression must return VkResult!"); \
    if (res != VK_SUCCESS) { \
        CHE_FATAL("Vulkan error: {0}; {1}", string_VkResult(res), message); \
    } \
}