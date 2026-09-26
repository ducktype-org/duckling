#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <os_utils/dynamic_library.hpp>

#include <expected>
#include <span>
#include <string>

namespace vm::native {

	/**
	 * @brief Links a dynamic library into the current process,
	 * allows to find where the symbols in it live.
	 * @details It is a wrapper over os_utils native library operations.
	 */
	struct DynamicLibrary final {
		DynamicLibrary(const DynamicLibrary&)            = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		DynamicLibrary(DynamicLibrary&&) noexcept;
		DynamicLibrary& operator=(DynamicLibrary&&) noexcept;
		~DynamicLibrary() noexcept;

		byte*                                             findSymbol(const char* name) const;
		base::Optional<byte*>                             maybeFindSymbol(const char* name) const;
		static std::expected<DynamicLibrary, std::string> fromMemory(
			std::span<const byte> library_bytes
		);
		static std::expected<DynamicLibrary, std::string> tryFromFile(const char* path);

	private:
		explicit DynamicLibrary(os_utils::NativeLibrary native_lib): lib{ native_lib } {}

		DynamicLibrary(): lib{ .handle = nullptr } {}

		os_utils::NativeLibrary lib;
	};
}
