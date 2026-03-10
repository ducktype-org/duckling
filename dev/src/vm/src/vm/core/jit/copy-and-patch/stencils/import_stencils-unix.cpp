#include "import_stencils.hpp"

namespace vm::jit {
	template<size_t BinarySize, size_t NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> LoadedStencils<BinarySize, NumFunctions>::load(
		StencilsT stencils
	) {
		int fd = memfd_create("lib", 0);
		// CORE_ASSERT(0 < fd, "memfd_create failed", std::strerror(fd));

		if (write(fd, stencils.binary.data(), stencils.binary.size())
		    != (ssize_t) stencils.binary.size()) {
			perror("write");
			close(fd);
			assert(false);
		}
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			close(fd);
			assert(false);
		}
		return LoadedStencils{ fd, handle, std::move(stencils) };
	}

	template<size_t BinarySize, size_t NumFunctions>
	template<class T>
	inline T* vm::jit::LoadedStencils<BinarySize, NumFunctions>::findSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		if (!sym_loc) {
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			assert(false);
		}
		return reinterpret_cast<T*>(sym_loc);
	}
}
