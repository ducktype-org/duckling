#pragma once

#include <base/types/ints.hpp>

#include <cstddef>

namespace vm::jit::cnp {

	struct JitMemory {
		static JitMemory allocate(usize size);

		void markExecutable();
		void freeJitMemory();

		template<class Func>
		const Func* intoFunc() const {
			return reinterpret_cast<const Func*>(memory);
		}

		byte* memory;
		usize size;
	};

}
