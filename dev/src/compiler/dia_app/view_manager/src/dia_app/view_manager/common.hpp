#pragma once
#include <functional>
#include <memory>

namespace dia {
	// By default, all pointers are shared.
	template<typename T>
	using ptr = std::shared_ptr<T>;

	const std::string CHILDREN     = "children";
	const std::string CONTENT      = "content";
	const std::string INTERMEDIATE = "intermediate";
	const std::string METADATA     = "metadata";
	const std::string TEXT         = "text";
	const std::string TYPE         = "type";
}  // namespace dia
