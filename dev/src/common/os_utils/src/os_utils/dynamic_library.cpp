#include "dynamic_library.hpp"

#include <base/config/target_info.hpp>
#include <base/str/str_utils.hpp>

#include <cerrno>
#include <cstdlib>
#include <expected>
#include <string>

#if BASE_TARGET_PLATFORM_POSIX

	#include <dlfcn.h>
	#include <sys/mman.h>
	#include <unistd.h>

namespace os_utils {

	namespace {
		// dlerror() can return null; never build a std::string from it.
		std::string dlErrorMessage() {
			const char* msg = dlerror();  // NOLINT(concurrency-mt-unsafe)
			if (msg) return { msg };
			return "unknown dlopen/dlsym error";
		}
	}

	std::expected<NativeLibrary, std::string> openLibrary(const char* path) {
		void* handle = dlopen(path, RTLD_NOW);
		if (!handle)
			return std::unexpected<std::string>(base::strConcat("dlopen failed: ", dlErrorMessage())
			);
		return NativeLibrary{ .handle = handle };
	}

	std::expected<NativeLibrary, std::string> openLibraryFromMemory(
		std::span<const byte> library_bytes
	) {
	#if BASE_TARGET_OS_MACOS
		// No memfd or /proc/self/fd on macOS; use a temp file, unlinked after load.
		std::string tmp_path = "/tmp/duckling_lib_XXXXXX";
		int         fd       = mkstemp(tmp_path.data());
		if (fd == -1) return std::unexpected<std::string>("mkstemp failed");
	#else
		int fd = memfd_create("lib", 0);
		if (fd == -1) return std::unexpected<std::string>("memfd_create failed");
	#endif

		auto write_n = [&]() -> std::expected<void, std::string> {
			usize to_write = library_bytes.size();
			auto  ptr      = library_bytes.data();
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
				if (ret == -1) {
					if (errno == EINTR) continue;  // interrupted by a signal, retry
					// Clean up before returning: the fd is no longer usable.
	#if BASE_TARGET_OS_MACOS
					unlink(tmp_path.c_str());
	#endif
					close(fd);
					return std::unexpected<std::string>("write failed");
				}
				auto written = static_cast<usize>(ret);
				to_write -= written;
				ptr += written;
			}
			return {};
		};

		return write_n().and_then([&]() -> std::expected<NativeLibrary, std::string> {
	#if BASE_TARGET_OS_MACOS
			// dlopen reads from the path, not the fd; no lseek needed.
			void* handle = dlopen(tmp_path.c_str(), RTLD_NOW);  // NOLINT(concurrency-mt-unsafe)
			// The loaded image stays valid without the temp file; clean up before checking the result.
			unlink(tmp_path.c_str());
			close(fd);
	#else
			// Rewind so dlopen (via /proc/self/fd/N) reads from the start.
			if (lseek(fd, 0, SEEK_SET) == -1) {
				close(fd);
				return std::unexpected<std::string>("lseek failed");
			}
			auto  path   = base::strConcat("/proc/self/fd/", fd);
			void* handle = dlopen(path.data(), RTLD_NOW);
			// The loaded image stays valid without the fd; close before checking the result.
			close(fd);
	#endif
			if (!handle)
				return std::unexpected<std::string>(
					base::strConcat("dlopen failed: ", dlErrorMessage())
				);
			return NativeLibrary{ .handle = handle };
		});
	}

	std::expected<void*, std::string> findSymbol(const NativeLibrary& lib, const char* name) {
		dlerror();                        // NOLINT(concurrency-mt-unsafe), clear stale errors
		void* sym = dlsym(lib.handle, name);
		if (const char* err = dlerror())  // NOLINT(concurrency-mt-unsafe)
			return std::unexpected<std::string>(err);
		return sym;
	}

	void closeLibrary(NativeLibrary& lib) {
		if (lib.handle) dlclose(lib.handle);
		lib = NativeLibrary{};
	}
}

#else
namespace os_utils {
	// Unsupported platforms compile but return errors at runtime.
	// @TODO: #3343 Add Windows CI coverage for os_utils and filepath_utils platform branches.
	std::expected<NativeLibrary, std::string> openLibrary(const char*) {
		return std::unexpected<std::string>(
			"os_utils::dynamic_library: not implemented on this platform"
		);
	}

	std::expected<NativeLibrary, std::string> openLibraryFromMemory(std::span<const byte>) {
		return std::unexpected<std::string>(
			"os_utils::dynamic_library: not implemented on this platform"
		);
	}

	std::expected<void*, std::string> findSymbol(const NativeLibrary&, const char*) {
		return std::unexpected<std::string>(
			"os_utils::dynamic_library: not implemented on this platform"
		);
	}

	void closeLibrary(NativeLibrary& lib) { lib = NativeLibrary{}; }
}
#endif
