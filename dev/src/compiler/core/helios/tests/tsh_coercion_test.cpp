#include <helios/tsh/coercions/reference_coercion.hpp>
#include <helios/tsh/types.hpp>

#include <tester/tester.hpp>

#include <utility>
#include <vector>

using namespace compiler::tsh;

/**
 * Tests of what the type system decides about coercions.
 */
class HigherTypeSystemCoercionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HigherTypeSystemCoercionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(referenceCoercionTable); }

private:
	void referenceCoercionTable() {
		using enum ReferenceKind;
		using enum ReferenceAdjustment;

		// --- From Direct ---
		// Direct -> Direct
		const auto d_d = referenceCoercionRule(Direct, Direct);
		assertTrue(d_d.adjustment == None, "Direct -> Direct: adjustment is None.");
		assertTrue(d_d.creates_new_value, "Direct -> Direct: creates a new value (copy).");
		assertFalse(
			d_d.points_to_source, "Direct -> Direct: the copy does not point to the source."
		);

		// Direct -> Ref
		const auto d_r = referenceCoercionRule(Direct, Ref);
		assertTrue(d_r.adjustment == Illegal, "Direct -> Ref: adjustment is Illegal.");
		assertFalse(d_r.creates_new_value, "Direct -> Ref: does not create a value.");
		assertFalse(d_r.points_to_source, "Direct -> Ref: does not point to the source.");

		// Direct -> Box
		const auto d_b = referenceCoercionRule(Direct, Box);
		assertTrue(d_b.adjustment == Illegal, "Direct -> Box: adjustment is Illegal.");
		assertFalse(d_b.creates_new_value, "Direct -> Box: does not create a value.");
		assertFalse(d_b.points_to_source, "Direct -> Box: does not point to the source.");


		// --- From Ref ---
		// Ref -> Direct
		const auto r_d = referenceCoercionRule(Ref, Direct);
		assertTrue(r_d.adjustment == Deref, "Ref -> Direct: adjustment is Deref.");
		assertTrue(
			r_d.creates_new_value, "Ref -> Direct: creates a new value by reading the pointer."
		);
		assertFalse(
			r_d.points_to_source, "Ref -> Direct: the new value does not point to the source."
		);

		// Ref -> Ref
		const auto r_r = referenceCoercionRule(Ref, Ref);
		assertTrue(r_r.adjustment == None, "Ref -> Ref: adjustment is None.");
		assertFalse(r_r.creates_new_value, "Ref -> Ref: does not create a new value, only rebinds.");
		assertTrue(
			r_r.points_to_source, "Ref -> Ref: the new reference points to the original source."
		);

		// Ref -> Box
		const auto r_b = referenceCoercionRule(Ref, Box);
		assertTrue(r_b.adjustment == Illegal, "Ref -> Box: adjustment is Illegal.");
		assertFalse(r_b.creates_new_value, "Ref -> Box: does not create a value.");
		assertFalse(r_b.points_to_source, "Ref -> Box: does not point to the source.");


		// --- From Box ---
		// Box -> Direct
		const auto b_d = referenceCoercionRule(Box, Direct);
		assertTrue(b_d.adjustment == Deref, "Box -> Direct: adjustment is Deref.");
		assertTrue(
			b_d.creates_new_value, "Box -> Direct: creates a new value by reading the pointer."
		);
		assertFalse(
			b_d.points_to_source, "Box -> Direct: the new value does not point to the source."
		);

		// Box -> Ref
		const auto b_r = referenceCoercionRule(Box, Ref);
		assertTrue(b_r.adjustment == Illegal, "Box -> Ref: adjustment is Illegal.");
		assertFalse(b_r.creates_new_value, "Box -> Ref: does not create a value.");
		assertFalse(b_r.points_to_source, "Box -> Ref: does not point to the source.");

		// Box -> Box
		const auto b_b = referenceCoercionRule(Box, Box);
		assertTrue(b_b.adjustment == None, "Box -> Box: adjustment is None.");
		assertTrue(b_b.creates_new_value, "Box -> Box: creates a new value by moving.");
		assertFalse(b_b.points_to_source, "Box -> Box: the moved box does not point to the source.");
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
