#pragma once

#include <memory>

template<class T>
using Ref = std::shared_ptr<T>;

template<class T, class... Args>
Ref<T> MakeRef(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<class T>
using Scope = std::unique_ptr<T>;

template<class T, class... Args>
Scope<T> MakeScope(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<class T, class... Args>
Ref<T> CreateRef(Args&&... args)
{
    return MakeRef<T>(std::forward<Args>(args)...);
}

template<class T, class... Args>
Scope<T> CreateScope(Args&&... args)
{
    return MakeScope<T>(std::forward<Args>(args)...);
}
