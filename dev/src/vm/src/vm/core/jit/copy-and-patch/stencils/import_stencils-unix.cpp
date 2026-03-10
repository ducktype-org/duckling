#include "import_stencils.hpp"

#include <base/except/exceptions.hpp>

namespace vm::jit {
	template<size_t BinarySize, size_t NumFunctions>
	inline LoadedStencils<BinarySize, NumFunctions> LoadedStencils<BinarySize, NumFunctions>::load(
		StencilsT stencils
	) {
		int fd = memfd_create("lib", 0);
		CORE_ASSERT(0 < fd, "memfd_create failed: ", std::strerror(errno));

		auto write_n = [&]() {
			size_t to_write = stencils.binary.size();
			const std::byte* ptr = reinterpret_cast<const std::byte*>(stencils.binary.data());
			while (to_write) {
				ssize_t ret = write(fd, ptr, to_write);
				CORE_ASSERT(ret != -1, "write failed: ", std::strerror(errno));
				size_t written = (size_t)ret;
				CORE_ASSERT(written <= to_write, "write failed??");
				to_write -= written;
				ptr += written;
				std::cerr << written << "\n";
			}
		};
		write_n();
		lseek(fd, 0, SEEK_SET);

		char path[64];
		sprintf(path, "/proc/self/fd/%d", fd);
		void* handle = dlopen(path, RTLD_NOW);
		CORE_ASSERT(handle, "dlopen failed: ", dlerror());

		return LoadedStencils{ fd, handle, std::move(stencils) };
	}

	template<size_t BinarySize, size_t NumFunctions>
	template<class T>
	inline T* vm::jit::LoadedStencils<BinarySize, NumFunctions>::findSymbol(const char* name) const {
		void* sym_loc = dlsym(lib_handle, name);
		CORE_ASSERT(sym_loc, "dlsym failed: ", dlerror());
		assert(sym_loc);
		return reinterpret_cast<T*>(sym_loc);
	}
}
