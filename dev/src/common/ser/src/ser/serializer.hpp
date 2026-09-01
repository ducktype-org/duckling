#pragma once

#include <ser/concepts.hpp>
#include <ser/errc.hpp>

namespace ser {

	/**
	 * @brief The extension point for types you do not own. Specialize it with any ONE of:
	 *
	 *     visit(auto& ar, auto& self)                symmetric, object exists
	 *     write(writer auto&, const T&) + read(reader auto&, T&)
	 *     write(writer auto&, const T&) + make(reader auto&) -> T
	 *
	 * A specialization outranks every in-class hook and every builtin rule, which is what
	 * lets you override a type you cannot edit. The primary template is defined and empty on
	 * purpose, so `serializer<T>::write` simply does not exist for unspecialized T and the
	 * detectors report false instead of hitting an incomplete type.
	 */
	template<class T>
	struct serializer {};

} /* namespace ser */
