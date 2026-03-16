#pragma once

#include <cstddef>

namespace vm::jit::cnp {
	enum class HoleType;

	struct StencilHole {
		int      offset;
		int      size;
		HoleType type;

		void relocate(byte* from, byte* to) const;
	};

#ifdef __x86_64__
	enum class HoleType { Movable };

	void StencilHole::relocate(byte* from, byte* to) const {
		switch (type) {
			break;
		case HoleType::Movable:
			*reinterpret_cast<int*>(to + offset) += (to - from);
			break;
		}
	}
#else
	#error "Relocation types unknown on your architecture"
#endif
}
