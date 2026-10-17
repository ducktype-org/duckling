#pragma once

#include <ser/concepts.hpp>
#include <ser/errc.hpp>

namespace ser {

	/**
	 * @brief Where a type says how it is serialized. There are three places, asked in this
	 * order, and the first one that has a hook wins:
	 *
	 *   1. a `ser::Serializer<T>` specialization (this template) - for a type you cannot edit,
	 *      which is how every std and base adapter is written
	 *   2. hooks in the class itself (`serVisit`, `serWrite` + `serRead` / `serMake`, or
	 *      `SER_DESCRIBE`) - for your own types, and the only way to reach private members
	 *   3. free `serVisit` / `serWrite` / ... found by ADL - for a type in another namespace
	 *      that would rather not specialize a ser template
	 *
	 * A type with none of them is walked field by field. Each place takes one of these forms:
	 *
	 *     visit(auto& ar, auto& self)                symmetric, object exists
	 *     write(writer auto&, const T&) + read(reader auto&, T&)
	 *     write(writer auto&, const T&) + make(reader auto&) -> T
	 *
	 * The primary template is empty, so `Serializer<T>::write` does not exist for an
	 * unspecialized T and the detectors answer false instead of hitting an incomplete type.
	 */
	template<class T>
	struct Serializer {};

}  // namespace ser
