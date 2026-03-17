#pragma once

#include <base/types/ints.hpp>

#include <span>


#if __unix__

namespace vm::jit::cnp {
	struct DynamicLibrary {
		DynamicLibrary()                                 = delete;
		DynamicLibrary(const DynamicLibrary&)            = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		DynamicLibrary(DynamicLibrary&&)            = default;
		DynamicLibrary& operator=(DynamicLibrary&&) = default;

		std::byte*            findSymbol(const char* name) const;
		static DynamicLibrary load(std::span<const byte> binary);

	private:
		DynamicLibrary(int _lib_fd, void* _lib_handle):
			  lib_fd{ _lib_fd },
			  lib_handle{ _lib_handle } {}

		int   lib_fd;
		void* lib_handle;
	};
}

#else
	#error "Unsupported system"
#endif
