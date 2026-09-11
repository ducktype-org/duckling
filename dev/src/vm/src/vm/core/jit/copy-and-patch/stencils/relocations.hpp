#pragma once

#include <base/config/target_info.hpp>
#include <base/types/ints.hpp>

#include <cstddef>

namespace vm::jit::cnp {
	enum class HoleType;

	struct StencilHole final {
		int      offset;
		int      size;
		HoleType type;

		void relocate(const byte* from, byte* to) const;
	};

#if BASE_TARGET_ARCH_X86 && BASE_TARGET_ARCH_64
	enum class HoleType { Movable };

	void StencilHole::relocate(const byte* from, byte* to) const {
		switch (type) {
			break;
		case HoleType::Movable:
			*reinterpret_cast<i64*>(to + offset) += (to - from);
			break;
		}
	}
#else
	#error "Relocation types unknown on your architecture"
#endif
}
