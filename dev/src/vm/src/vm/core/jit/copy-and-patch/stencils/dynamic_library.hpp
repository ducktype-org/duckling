#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <span>


#if __unix__

namespace vm::jit::cnp {

	/**
	 * @brief Links an in-memory dynamic library into the current process,
	 * allows to find where the symbols in it live.
	 * @details It is a wrapper over a system linker.
	 */
	struct DynamicLibrary {
		DynamicLibrary(const DynamicLibrary&)            = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		DynamicLibrary(DynamicLibrary&&) noexcept;
		DynamicLibrary& operator=(DynamicLibrary&&) noexcept;
		~DynamicLibrary() noexcept;

		std::byte*                                        findSymbol(const char* name) const;
		base::Optional<std::byte*>                        maybeFindSymbol(const char* name) const;
		static std::expected<DynamicLibrary, std::string> fromMemory(
			std::span<const byte> library_bytes
		);

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
