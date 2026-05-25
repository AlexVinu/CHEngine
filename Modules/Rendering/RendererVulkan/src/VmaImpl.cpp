// Single translation unit that provides the VMA implementation.
// All other files just include vk_mem_alloc.h without VMA_IMPLEMENTATION.
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif
