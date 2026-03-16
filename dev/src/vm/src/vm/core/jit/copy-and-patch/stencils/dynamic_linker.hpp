#pragma once

#include <span>


#if __unix__

namespace vm::jit::cnp {
	struct DynamicLibrary {
		int   lib_fd;
		void* lib_handle;

		template<class T = std::byte>
		T* findSymbol(const char* name) const;

		static DynamicLibrary load(std::span<const std::byte> binary);
	};
}

#else
	#error "Unsupported system"
#endif
