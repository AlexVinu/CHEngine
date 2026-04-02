#pragma once

#include<memory>

namespace CHEngine
{
	template<class T>
	using Ref = std::shared_ptr<T>;

	template<class T>
	using Scope = std::unique_ptr<T>;
}