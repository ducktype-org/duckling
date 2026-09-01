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

		const auto rule = [](const ReferenceKind from, const ReferenceKind to) {
			return referenceCoercionRule(from, to);
		};

		assertTrue(rule(Direct, Direct).adjustment == None, "Direct to direct does nothing.");
		assertTrue(rule(Direct, Direct).creates_new_value, "A direct value is copied.");
		assertFalse(rule(Direct, Direct).points_to_source, "A copy does not point to the source.");

		assertTrue(rule(Ref, Direct).adjustment == Deref, "Reading out of a `ref` derefs.");
		assertTrue(rule(Box, Direct).adjustment == Deref, "Reading out of a `box` derefs.");
		assertTrue(rule(Ref, Direct).creates_new_value, "A pointee read out is a new value.");
		assertTrue(rule(Box, Direct).creates_new_value, "A pointee read out is a new value.");

		assertTrue(rule(Ref, Ref).adjustment == None, "Rebinding a `ref` does nothing.");
		assertFalse(rule(Ref, Ref).creates_new_value, "Rebinding a `ref` copies nothing.");
		assertTrue(rule(Ref, Ref).points_to_source, "A rebound `ref` still aliases its source.");

		assertTrue(rule(Box, Box).adjustment == None, "Box to box does nothing.");
		assertTrue(rule(Box, Box).creates_new_value, "A box hands over the whole value.");
		assertFalse(rule(Box, Box).points_to_source, "A handed over box shares nothing.");

		for (const auto [from, to]: std::vector<std::pair<ReferenceKind, ReferenceKind>>{
				 { Direct, Ref }, { Direct, Box }, { Ref, Box }, { Box, Ref } })
			assertFalse(
				rule(from, to).isLegal(), "Taking a reference has to be written out explicitly."
			);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
