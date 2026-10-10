// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "reference_coercion.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::tsh::coercions {
	ReferenceCoercion referenceCoercionRule(const ReferenceKind from, const ReferenceKind to) {
		using enum ReferenceKind;
		using enum ReferenceAdjustment;


		const auto legal = [](ReferenceAdjustment op, bool creates_new_value, bool points_to_source
		                   ) -> ReferenceCoercion {
			return ReferenceCoercion{
				.adjustment        = op,
				.creates_new_value = creates_new_value,
				.points_to_source  = points_to_source,
			};
		};

		const auto illegal = ReferenceCoercion{
			.adjustment        = ReferenceAdjustment::Illegal,
			.creates_new_value = false,
			.points_to_source  = false,
		};

		switch (from) {
		case Direct:
			// `Direct` -> `Direct` copies and doesn't point to source.
			if (to == Direct) return legal(None, true, false);
			return illegal;
		case Ref:
			// `Ref` -> `Direct` reads out the pointee, thus creates a new value, doesn't point to
			// source.
			if (to == Direct) return legal(Deref, true, false);
			// `Ref` -> `Ref` just copies the reference, points to source.
			if (to == Ref) return legal(None, false, true);
			return illegal;
		case Box:
			// `Box` -> `Direct` reads out the pointee, thus creates a new value, doesn't point to
			// source.
			if (to == Direct) return legal(Deref, true, false);
			// `Box` -> `Box` can either `move` the box or deep copy the existing one, thus it
			// creates a new value and never points to source.
			if (to == Box) return legal(None, true, false);
			return illegal;
		}

		CORE_UNREACHABLE();
	}

	std::string ReferenceCoercion::toString() const {
		if (not isLegal()) return "illegal";

		std::string out = adjustment == ReferenceAdjustment::Deref ? "deref" : "none";
		if (creates_new_value) out += ", new value";
		if (points_to_source) out += ", aliases source";
		return out;
	}
}
