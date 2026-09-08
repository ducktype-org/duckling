#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <expected>
#include <span>
#include <string>


#if defined(__unix__) || defined(__APPLE__)

namespace vm::native {

	/**
	 * @brief Links a dynamic library into the current process,
	 * allows to find where the symbols in it live.
	 * @details It is a wrapper over a system linker.
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
		DynamicLibrary(int in_lib_fd, void* in_lib_handle):
			  lib_fd{ in_lib_fd },
			  lib_handle{ in_lib_handle } {}

		DynamicLibrary(): DynamicLibrary(-1, nullptr) {}

		int   lib_fd;
		void* lib_handle;
	};
}

#else
	#error "Unsupported system"
#endif
