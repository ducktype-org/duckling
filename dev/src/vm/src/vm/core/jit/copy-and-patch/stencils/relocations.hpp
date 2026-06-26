#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/types/ints.hpp>

#include <concepts>
#include <cstddef>

MAKE_STRINGIFYABLE_ENUM(vm::jit::cnp, u32, HoleValue,
	InstrPtr,
	Arg0,
	Arg1,
	ContinueFn,
	JmpFn,
	CallFn,
	CallOpcode,
	ExceptionThrower,
	None
);

namespace vm::jit::cnp {
	struct StencilHole {
		int       offset;
		HoleValue value;

		template<std::integral T>
		inline void patch(byte* new_addr, T value) const;
	};

#ifdef __x86_64__
	template<std::integral T>
	void StencilHole::patch(byte* new_addr, T value) const {
		*reinterpret_cast<T*>(new_addr + offset) += value;
	}
#else
	#error "Patching unimplemented on your architecture"
#endif
}
