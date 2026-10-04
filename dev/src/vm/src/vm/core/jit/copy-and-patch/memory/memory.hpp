#pragma once

#include <base/types/ints.hpp>

#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace vm::jit::cnp {

	struct JitFuncMemory final {
		JitFuncMemory(const JitFuncMemory&)            = delete;
		JitFuncMemory& operator=(const JitFuncMemory&) = delete;

		JitFuncMemory(JitFuncMemory&& other) noexcept: JitFuncMemory() { swap(*this, other); }

		JitFuncMemory& operator=(JitFuncMemory&& other) noexcept {
			swap(*this, other);
			return *this;
		}

		static std::expected<JitFuncMemory, std::string> allocate(usize size);
		~JitFuncMemory() noexcept;

		std::expected<void, std::string> markExecutable();
		void                             dump(const char* filename);

		[[nodiscard]] std::span<byte> span() const { return std::span{ addr, size }; }

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

		JitFuncMemory(): addr{ nullptr }, size{ 0 } {}

		static void swap(JitFuncMemory& a, JitFuncMemory& b) {
			std::swap(a.addr, b.addr);
			std::swap(a.size, b.size);
		}
	};
}
