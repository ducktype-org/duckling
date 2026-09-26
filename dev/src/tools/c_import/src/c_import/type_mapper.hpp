#pragma once

#include "c_decls.hpp"

#include <string>

namespace c_import {

	/**
	 * @brief Renders a type as Duckling source.
	 *
	 * Unsupported types render as an empty string; callers must check @ref isSupported first.
	 */
	std::string renderType(const CType& type);

	/**
	 * @brief Whether a type has an admissible Duckling spelling.
	 *
	 * A record is unsupported when any of its fields is, so this recurses through arrays and
	 * record fields. Pointers are always supported: a pointer to something skipped degrades to
	 * `cptr u8`.
	 */
	bool isSupported(const CType& type);

	/** The reason the type was skipped, or an empty string when it is supported. */
	std::string unsupportedReason(const CType& type);

	std::string renderScalar(Scalar scalar);

}
