#pragma once

#include <base/types/ints.hpp>

#include <cstddef>

namespace vm::jit::cnp {

	struct JitMemory {
		static JitMemory allocate(usize size);

		void mark_executable();
		void free_jit_memory();

		template<class Func>
		const Func* into_func() const {
			return reinterpret_cast<const Func*>(memory);
		}

		byte* memory;
		usize size;
	};

}
