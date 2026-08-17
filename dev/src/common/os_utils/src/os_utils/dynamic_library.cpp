#include "dynamic_library.hpp"

#include <base/str/str_utils.hpp>

#include <cerrno>
#include <expected>
#include <string>

#if defined(__unix__) || defined(__APPLE__)

	#include <dlfcn.h>
	#include <sys/mman.h>
	#include <unistd.h>

namespace os_utils {

	namespace {
		// dlerror() returns null if no error occurred since the last call on this
		// thread. Never construct std::string from a null pointer.
		std::string dlErrorMessage() {
			const char* msg = dlerror();  // NOLINT(concurrency-mt-unsafe)
			if (msg) return { msg };
			return "unknown dlopen/dlsym error";
		}
	}

	std::expected<NativeLibrary, std::string> openLibrary(const char* path) {
		void* handle = dlopen(path, RTLD_NOW);
		if (!handle) return std::unexpected(dlErrorMessage());
		return NativeLibrary{ .handle = handle };
	}

	std::expected<NativeLibrary, std::string> openLibraryFromMemory(
		std::span<const byte> library_bytes
	) {
	#if defined(__APPLE__)
		// macOS has neither memfd_create nor /proc/self/fd, and dlopen requires a real
		// path, so stage the library in a temp file that is unlinked once it is loaded.
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
					if (errno == EINTR) continue;  // interrupted by a signal — retry
					// Clean up before returning: the fd is no longer usable.
	#if defined(__APPLE__)
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
	#if defined(__APPLE__)
			// dlopen reads from the temp file path, not from fd — no lseek needed.
			void* handle = dlopen(tmp_path.c_str(), RTLD_NOW);  // NOLINT(concurrency-mt-unsafe)
			// dlopen has read the file, so the backing file and fd are no longer needed regardless
			// of the outcome (the loaded image stays valid without them). Clean up before checking
			// the result so a failed dlopen does not leak the temp file.
			unlink(tmp_path.c_str());
			close(fd);
	#else
			// The fd was written from offset 0; rewind so dlopen (via /proc/self/fd/N)
			// reads the library from the beginning.
			if (lseek(fd, 0, SEEK_SET) == -1) {
				close(fd);
				return std::unexpected<std::string>("lseek failed");
			}
			auto  path   = base::strConcat("/proc/self/fd/", fd);
			void* handle = dlopen(path.data(), RTLD_NOW);
			// dlopen has read the library, so the fd is no longer needed regardless
			// of the outcome (the loaded image stays valid without it). Close before
			// checking the result so a failed dlopen does not leak the memfd.
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
		dlerror();                        // NOLINT(concurrency-mt-unsafe) — clear stale error state
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
	#error "Unsupported system"
#endif
