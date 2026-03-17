#pragma once

#include <base/types/ints.hpp>

#include <cstddef>

namespace vm::jit::cnp {

	struct JitFuncMemory {
		JitFuncMemory()                            = delete;
		JitFuncMemory(const JitFuncMemory&)            = delete;
		JitFuncMemory& operator=(const JitFuncMemory&) = delete;

		JitFuncMemory(JitFuncMemory&&)            = default;
		JitFuncMemory& operator=(JitFuncMemory&&) = default;

		static JitFuncMemory allocate(usize size);
		~JitFuncMemory() noexcept;

		void markExecutable();

		/**
		 * @brief Access the memory as if it was a function pointer. This is not compliant with the
		 * c++ standard, and can't be as c++ cannot guarantee that whatever was written inside is
		 * actually a function.
		 */
		template<class Func>
		requires std::is_function_v<Func> const Func* intoFunc() const {
			return reinterpret_cast<const Func*>(addr);
		}

		byte* addr;
		usize size;

	private:
		JitFuncMemory(byte* _addr, usize _size): addr{ _addr }, size{ _size } {}
	};

}
