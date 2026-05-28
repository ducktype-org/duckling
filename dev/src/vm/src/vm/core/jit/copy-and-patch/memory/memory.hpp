#pragma once

#include <base/types/ints.hpp>

#include <cstddef>
#include <span>

namespace vm::jit::cnp {

	struct JitFuncMemory {
		JitFuncMemory()                                = delete;
		JitFuncMemory(const JitFuncMemory&)            = delete;
		JitFuncMemory& operator=(const JitFuncMemory&) = delete;

		JitFuncMemory(JitFuncMemory&&)            = default;
		JitFuncMemory& operator=(JitFuncMemory&&) = default;

		static JitFuncMemory allocate(usize size);
		~JitFuncMemory() noexcept;

		void markExecutable();
		void dump(const char* filename);
		std::span<byte> span() {
			return std::span{addr, size};
		}

		/**
		 * @brief Access the memory as if it was a function pointer. This is not compliant with the
		 * c++ standard, and can't be as c++ cannot guarantee that whatever was written inside is
		 * actually a function.
		 */
		template<class Func>
		requires std::is_function_v<Func> [[nodiscard]] const Func* intoFunc() const {
			return reinterpret_cast<const Func*>(addr);
		}

		byte* addr;
		usize size;

	private:
		JitFuncMemory(byte* in_addr, usize in_size): addr{ in_addr }, size{ in_size } {}
	};

}
