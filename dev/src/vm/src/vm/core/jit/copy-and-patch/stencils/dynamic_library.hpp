#pragma once

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
		DynamicLibrary()                                 = delete;
		DynamicLibrary(const DynamicLibrary&)            = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		DynamicLibrary(DynamicLibrary&&)            = default;
		DynamicLibrary& operator=(DynamicLibrary&&) = default;

		std::byte*            findSymbol(const char* name) const;
		static DynamicLibrary load(std::span<const byte> binary);

	private:
		DynamicLibrary(int in_lib_fd, void* in_lib_handle):
			  lib_fd{ in_lib_fd },
			  lib_handle{ in_lib_handle } {}

		int   lib_fd;
		void* lib_handle;
	};
}

#else
	#error "Unsupported system"
#endif
