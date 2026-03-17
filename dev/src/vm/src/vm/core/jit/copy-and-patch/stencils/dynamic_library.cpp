#include "dynamic_library.hpp"

#include <base/except/exceptions.hpp>

#if __unix__

	#include <dlfcn.h>
	#include <sys/mman.h>
	#include <unistd.h>

	#include <cstring>
	#include <format>

namespace vm::jit::cnp {
	DynamicLibrary DynamicLibrary::load(std::span<const byte> binary) {
		int fd = memfd_create("lib", 0);
		SYSTEM_CHECK(fd != -1, "memfd_create failed:");

		auto write_n = [&]() {
			usize to_write = binary.size();
			auto  ptr      = binary.data();
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
				SYSTEM_CHECK(ret != -1, "write failed: ");
				auto written = static_cast<usize>(ret);
				to_write -= written;
				ptr += written;
			}
		};
		write_n();
		lseek(fd, 0, SEEK_SET);


		auto  path   = std::format("/proc/self/fd/{}", fd);
		void* handle = dlopen(path.data(), RTLD_NOW);
		CORE_ASSERT_STRONG(handle, "dlopen failed: ", dlerror());  // NOLINT(concurrency-mt-unsafe)

		return DynamicLibrary{fd, handle };
	}

	std::byte* DynamicLibrary::findSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		CORE_ASSERT_STRONG(sym_loc, "dlsym failed: ", dlerror());  // NOLINT(concurrency-mt-unsafe)
		return reinterpret_cast<std::byte*>(sym_loc);
	}
}

#else

	#error "Unsupported system"

#endif
