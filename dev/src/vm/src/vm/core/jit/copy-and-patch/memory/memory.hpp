#pragma once

#include <base/types/ints.hpp>

#include <expected>
#include <string>

namespace vm::jit::cnp {

	struct JitFuncMemory final {
		JitFuncMemory()                                = delete;
		JitFuncMemory(const JitFuncMemory&)            = delete;
		JitFuncMemory& operator=(const JitFuncMemory&) = delete;

		JitFuncMemory(JitFuncMemory&&) noexcept;
		JitFuncMemory& operator=(JitFuncMemory&&) noexcept;

		static std::expected<JitFuncMemory, std::string> allocate(usize size);
		~JitFuncMemory() noexcept;

		std::expected<void, std::string> markExecutable();

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
		JitFuncMemory(byte* in_addr, usize in_size): addr{ in_addr }, size{ in_size } {}
	};

}
