#include "dynamic_library.hpp"

#include <base/except/exceptions.hpp>

#if __unix__

	#include <dlfcn.h>
	#include <sys/mman.h>
	#include <unistd.h>

	#include <cstring>
	#include <format>

namespace vm::jit::cnp {
	DynamicLibrary DynamicLibrary::fromMemory(std::span<const byte> library_bytes) {
		int fd = memfd_create("lib", 0);
		CORE_SYSCALL_CHECK(fd != -1, "memfd_create failed:");

		auto write_n = [&]() {
			usize to_write = library_bytes.size();
			auto  ptr      = library_bytes.data();
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
			CORE_SYSCALL_CHECK(ret != -1, "write failed: ");
				auto written = static_cast<usize>(ret);
				to_write -= written;
				ptr += written;
			}
		};
		write_n();
		lseek(fd, 0, SEEK_SET);


		auto  path   = std::format("/proc/self/fd/{}", fd);
		void* handle = dlopen(path.data(), RTLD_NOW);
		CORE_SYSCALL_CHECK(handle, "dlopen failed: ", dlerror());  // NOLINT(concurrency-mt-unsafe)

		return DynamicLibrary{ fd, handle };
	}

	DynamicLibrary::DynamicLibrary(DynamicLibrary&& dynlib) noexcept:
		  lib_fd{ dynlib.lib_fd },
		  lib_handle{ dynlib.lib_handle } {
		dynlib.lib_fd     = -1;
		dynlib.lib_handle = nullptr;
	}

	DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& dynlib) noexcept {
		lib_fd     = dynlib.lib_fd;
		lib_handle = dynlib.lib_handle;

		dynlib.lib_fd     = -1;
		dynlib.lib_handle = nullptr;
		return *this;
	}

	DynamicLibrary::~DynamicLibrary() noexcept {
		if (lib_handle) dlclose(lib_handle);
		if (lib_fd != -1) close(lib_fd);
	}

	std::byte* DynamicLibrary::findSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		CORE_SYSCALL_CHECK(sym_loc, "dlsym failed: ", dlerror());  // NOLINT(concurrency-mt-unsafe)
		return reinterpret_cast<std::byte*>(sym_loc);
	}

	base::Optional<std::byte*> DynamicLibrary::maybeFindSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		if (sym_loc)
			return reinterpret_cast<std::byte*>(sym_loc);
		else
			return std::nullopt;
	}
}

#else

	#error "Unsupported system"

#endif
