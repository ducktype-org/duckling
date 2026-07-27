#include "dynamic_library.hpp"

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <expected>
#include <string>

#if defined(__unix__) || defined(__APPLE__)

	#include <dlfcn.h>
	#include <sys/mman.h>
	#include <unistd.h>

namespace os_utils {

	std::expected<NativeLibrary, std::string> openLibrary(const char* path) {
		void* handle = dlopen(path, RTLD_NOW);
		if (!handle) return std::unexpected(dlerror());  // NOLINT(concurrency-mt-unsafe)
		return NativeLibrary{ .handle = handle, .fd = -1 };
	}

	std::expected<NativeLibrary, std::string> openLibraryFromMemory(
		std::span<const byte> library_bytes
	) {
	#if defined(__APPLE__)
		// macOS has neither memfd_create nor /proc/self/fd, and dlopen requires a real
		// path, so stage the library in a temp file that is unlinked once it is loaded.
		std::string tmp_path = "/tmp/duckling_lib_XXXXXX";
		int         fd       = mkstemp(tmp_path.data());
		if (fd == -1) return std::unexpected<std::string>("mkstemp failed:");
	#else
		int fd = memfd_create("lib", 0);
		if (fd == -1) return std::unexpected<std::string>("memfd_create failed:");
	#endif

		auto write_n = [&]() -> std::expected<void, std::string> {
			usize to_write = library_bytes.size();
			auto  ptr      = library_bytes.data();
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
				if (ret == -1) return std::unexpected<std::string>("write failed");
				auto written = static_cast<usize>(ret);
				to_write -= written;
				ptr += written;
			}
			return {};
		};

		return write_n().and_then([&]() -> std::expected<NativeLibrary, std::string> {
			lseek(fd, 0, SEEK_SET);

	#if defined(__APPLE__)
			void* handle = dlopen(tmp_path.c_str(), RTLD_NOW);  // NOLINT(concurrency-mt-unsafe)
			// dlopen has read the file, so the backing file and fd are no longer needed regardless
			// of the outcome (the loaded image stays valid without them). Clean up before checking
			// the result so a failed dlopen does not leak the temp file.
			unlink(tmp_path.c_str());
			close(fd);
			fd = -1;
	#else
			auto  path   = base::strConcat("/proc/self/fd/", fd);
			void* handle = dlopen(path.data(), RTLD_NOW);
	#endif
			if (!handle)
				return std::unexpected<std::string>(
					base::strConcat("dlopen failed: ", dlerror())  // NOLINT(concurrency-mt-unsafe)
				);

			return NativeLibrary{ .handle = handle, .fd = fd };
		});
	}

	std::expected<void*, std::string> findSymbol(const NativeLibrary& lib, const char* name) {
		void* sym = dlsym(lib.handle, name);
		if (!sym) return std::unexpected<std::string>(dlerror());  // NOLINT(concurrency-mt-unsafe)
		return sym;
	}

	void closeLibrary(const NativeLibrary& lib) {
		if (lib.handle) dlclose(lib.handle);
		if (lib.fd != -1) close(lib.fd);
	}
}

#else
	#error "Unsupported system"
#endif
