#pragma once

#include <base/types/ints.hpp>

#include <concepts>
#include <cstddef>

namespace vm::jit::cnp {
	enum class HoleType;
	enum class HoleValue { Arg0, Arg1, ContinueFn, JmpFn, Zero, CallFn };

	struct StencilHole {
		int       offset;
		int       size;
		HoleType  type;
		HoleValue value;

		inline void relocate(const byte* from, byte* to) const;
		template<std::integral T>
		inline void patch(byte* new_addr, T value) const;
	};

#ifdef __x86_64__
	enum class HoleType { Movable };

	void StencilHole::relocate(const byte* from, byte* to) const {
		switch (type) {
			break;
		case HoleType::Movable:
			*reinterpret_cast<i64*>(to + offset) += (to - from);
			break;
		}
	}

	template<std::integral T>
	void StencilHole::patch(byte* new_addr, T value) const {
		*reinterpret_cast<T*>(new_addr + offset) += value;
	}
#else
	#error "Relocation types unknown on your architecture"
#endif
}
