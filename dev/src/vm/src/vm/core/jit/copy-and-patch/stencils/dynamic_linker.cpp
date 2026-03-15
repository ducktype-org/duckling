#include "dynamic_linker.hpp"

#include <base/except/exceptions.hpp>

#if __unix__

#include <cstring>

#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>

namespace vm::jit::cnp {
	inline DynamicLibrary DynamicLibrary::load(std::span<const char> binary) {
		int fd = memfd_create("lib", 0);
		SYSTEM_CHECK(fd != -1, "memfd_create failed:");

		auto write_n = [&]() {
			size_t           to_write = binary.size();
			auto ptr      = reinterpret_cast<const char*>(binary.data());
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
				SYSTEM_CHECK(ret != -1, "write failed: ");
				size_t written = (size_t) ret;
				to_write -= written;
				ptr += written;
			}
		};
		write_n();
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_NOW);
		CORE_CHECK(handle, "dlopen failed: ", dlerror());

		return DynamicLibrary{ fd, handle };
	}

	template<class T>
	inline T* DynamicLibrary::findSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		CORE_CHECK(sym_loc, "dlsym failed: ", dlerror());
		return reinterpret_cast<T*>(sym_loc);
	}
}

#else

#error "Unsupported system"

#endif
