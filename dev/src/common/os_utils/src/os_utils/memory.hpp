// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

#include <expected>
#include <string>

namespace os_utils {
	/**
	 * @brief Returns the operating system page size.
	 *
	 * The value is queried once and cached; subsequent calls are cheap and thread-safe.
	 */
	std::expected<usize, std::string> getPageSize();

	/**
	 * @brief Allocates a range of virtual memory with read+write permissions.
	 *
	 * @param size Number of bytes to allocate. Does not have to be page-aligned,
	 *             but the kernel rounds up internally.
	 * @return Pointer to the start of the mapping, or an error message.
	 *
	 * @note The returned memory is zero-initialized.
	 * @note The returned address is page-aligned by the OS, but the size is not
	 *       tracked by this API; the caller must remember the aligned size to
	 *       pass to freePages().
	 */
	std::expected<byte*, std::string> allocatePages(usize size);

	/**
	 * @brief Changes the protection of a mapping to read+execute (removes write).
	 *
	 * @param addr Start of the mapping, as returned by allocatePages().
	 * @param size Size of the mapping. Should match what was passed to allocatePages().
	 *
	 * @note Write the machine code before calling this function; the mapping is
	 *       no longer writable afterwards.
	 */
	std::expected<void, std::string> markExecutable(byte* addr, usize size);

	/**
	 * @brief Releases a mapping previously obtained from allocatePages().
	 *
	 * @param addr Start of the mapping. nullptr is a no-op.
	 * @param size Size of the mapping. Must match the value passed to allocatePages().
	 */
	void freePages(byte* addr, usize size);
}
