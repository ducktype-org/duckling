// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "coercion_tester.hpp"

#include <helios/tsh/coercions/best_coercion.hpp>
#include <helios/tsh/coercions/reference_coercion.hpp>
#include <helios/tsh/queries/types.hpp>

#include <base/str/str_utils.hpp>

#include <tester/tester.hpp>

#include <utility>
#include <vector>

using namespace coercion_testing;

/**
 * Tests of what the type system decides about coercions.
 */
class HigherTypeSystemCoercionTest final: public CoercionTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HigherTypeSystemCoercionTest

public:
	explicit HigherTypeSystemCoercionTest(tester::TestConfig config):
		  CoercionTestSuite(std::move(config), TESTER_SUITE_NAME) {
		TESTER_ADD_TEST(referenceKinds);
		TESTER_ADD_TEST(coercionRanks);
		TESTER_ADD_TEST(coercionPicking);
		TESTER_ADD_TEST(typeCoercions);
		TESTER_ADD_TEST(typeCoercionErrors);
		TESTER_ADD_TEST(ownership);
		TESTER_ADD_TEST(tupleLiterals);
	}

private:
	const SymbolType<> boolean           = direct(getBoolType());
	const SymbolType<> byte_type         = direct(getByteType());
	const SymbolType<> mutable_pointer   = direct(getRawPointerType(true));
	const SymbolType<> immutable_pointer = direct(getRawPointerType(false));

	void referenceKinds() {
		using enum ReferenceKind;
		using enum ReferenceAdjustment;

		struct Case final {
			ReferenceKind     from;
			ReferenceKind     into;
			ReferenceCoercion expected;
		};

		const std::vector<Case> table{
			{ .from = Direct,
			  .into = Direct,
			  .expected
			  = { .adjustment = None, .creates_new_value = true, .points_to_source = false } },
			{ .from = Direct, .into = Ref, .expected = ReferenceCoercion::illegal() },
			{ .from = Direct, .into = Box, .expected = ReferenceCoercion::illegal() },
			{ .from = Ref,
			  .into = Direct,
			  .expected
			  = { .adjustment = Deref, .creates_new_value = true, .points_to_source = false } },
			{ .from = Ref,
			  .into = Ref,
			  .expected
			  = { .adjustment = None, .creates_new_value = false, .points_to_source = true } },
			{ .from = Ref, .into = Box, .expected = ReferenceCoercion::illegal() },
			{ .from = Box,
			  .into = Direct,
			  .expected
			  = { .adjustment = Deref, .creates_new_value = true, .points_to_source = false } },
			{ .from = Box, .into = Ref, .expected = ReferenceCoercion::illegal() },
			{ .from = Box,
			  .into = Box,
			  .expected
			  = { .adjustment = None, .creates_new_value = true, .points_to_source = false } },
		};
		for (usize i = 0; i < table.size(); i++) {
			const ReferenceCoercion found = referenceCoercionRule(table[i].from, table[i].into);
			assertEqual(
				table[i].expected, found, base::strConcat("Row ", i, " is `", found.toString(), "`.")
			);
		}

		ASSERT_EQUAL(
			referenceCoercionRule(Ref, Direct),
			plan("ref i32", "i64").succeeds().get().referenceCoercion()
		);
		ASSERT_EQUAL(
			referenceCoercionRule(Ref, Direct),
			plan("ref i32", "i32 | i64").succeeds().get().referenceCoercion()
		);
		ASSERT_EQUAL(
			referenceCoercionRule(Ref, Ref),
			plan("ref i32", "i64 | ref i32").succeeds().get().referenceCoercion()
		);
		ASSERT_EQUAL(
			referenceCoercionRule(Box, Box),
			plan("box i32", "box i32").succeeds().get().referenceCoercion()
		);
	}

	void coercionRanks() {
		using enum Rank;

		const std::vector<Rank> best_to_worst{
			Identity,   MutabilityChange, Deref,     Numeric,    VariantPack,
			LiftToType, UserConversion,   ZeroCheck, RetypeVoid,
		};
		for (usize i = 1; i < best_to_worst.size(); i++) {
			const CoercionRank better = CoercionRank::of(best_to_worst[i - 1]);
			const CoercionRank worse  = CoercionRank::of(best_to_worst[i]);
			const std::string  pair   = base::strConcat("Ranks ", i - 1, " and ", i, ": ");

			assertTrue(better.dominates(worse), base::strConcat(pair, "the better one wins."));
			assertFalse(worse.dominates(better), base::strConcat(pair, "the worse one loses."));
			assertTrue(worse.ties(worse), base::strConcat(pair, "a rank ties with itself."));
			assertFalse(worse.ties(better), base::strConcat(pair, "different ranks do not tie."));
			assertEqual(
				best_to_worst[i],
				CoercionRank::combine(better, worse).getRank(),
				base::strConcat(pair, "doing both is as bad as the worse one.")
			);
		}
		ASSERT_EQUAL(true, CoercionRank::identity().isIdentity());
		ASSERT_EQUAL(false, CoercionRank::of(MutabilityChange).isIdentity());

		plan("i32", "i32").succeeds().ranks(Identity);
		plan("box i32", "box i32").succeeds().ranks(Identity);
		plan("(const i32, i32)", "(i32, i32)").succeeds().ranks(MutabilityChange);
		plan(mutable_pointer, immutable_pointer).succeeds().ranks(MutabilityChange);
		plan("box i32", "i32").succeeds().ranks(Deref);
		plan("ref i32", "i64").succeeds().ranks(Numeric);
		plan("(i32, i32)", "(i64, i64)").succeeds().ranks(Numeric);
		plan("i32", "i32 | bool").succeeds().ranks(VariantPack);
		plan("i32", "i64 | bool").succeeds().ranks(VariantPack);
		plan("(i32, i32)", "(i32 | bool, i64)").succeeds().ranks(VariantPack);
		plan("()", "type").succeeds().ranks(LiftToType);
		plan("char", "bool").succeeds().ranks(ZeroCheck);
		plan("char", "bool | i32").succeeds().ranks(ZeroCheck);
		plan("void", "i32").succeeds().ranks(RetypeVoid);
		plan("(void, i32)", "(i64, i64)").succeeds().ranks(RetypeVoid);

		// Deciding the ownership never changes the rank.
		coerce("makeBox()", "box i32").succeeds().ranks(Identity);
		coerce("makeBoxAndInt()", "(box i32, i64)").succeeds().ranks(Numeric);
	}

	void coercionPicking() {
		using namespace coercion_error;
		using namespace candidate_choice;
		using enum Rank;

		const auto choose = [](const std::vector<Rank>& ranks) {
			std::vector<Candidate> candidates;
			candidates.reserve(ranks.size());
			for (usize i = 0; i < ranks.size(); i++)
				candidates.push_back({ .id = i, .rank = CoercionRank::of(ranks[i]) });
			return bestCoercion(candidates);
		};
		ASSERT_MATCHES(choose({}), NoCandidate);
		const CandidateChoice only = choose({ ZeroCheck });
		ASSERT_MATCHES(only, Chosen);
		ASSERT_EQUAL(usize{ 0 }, v_get(only, Chosen).id);
		const CandidateChoice better = choose({ ZeroCheck, Numeric, ZeroCheck });
		ASSERT_MATCHES(better, Chosen);
		ASSERT_EQUAL(usize{ 1 }, v_get(better, Chosen).id);
		const CandidateChoice tied = choose({ Numeric, ZeroCheck, Numeric });
		ASSERT_MATCHES(tied, Tied);
		ASSERT_EQUAL((std::vector<usize>{ 0, 2 }), v_get(tied, Tied).candidates);

		plan("i32", "i32 | bool").succeeds().steps({ VARIANT_PACK }).packsInto("i32");
		plan("i32", "i32 | i64").succeeds().packsInto("i32");
		plan("i32", "i64 | bool").succeeds().steps({ NUMERIC, VARIANT_PACK }).packsInto("i64");
		plan("i32", "i64 | f64").succeeds().packsInto("i64");
		plan("i32", "bool | i64 | u8").succeeds().packsInto("i64");
		plan("char", "bool | i32").succeeds().steps({ ZERO_CHECK, VARIANT_PACK });
		plan(byte_type, variantOf({ boolean, type("u8") })).succeeds().packsInto("bool");
		plan(mutable_pointer, variantOf({ immutable_pointer, boolean }))
			.succeeds()
			.steps({ MUTABILITY_CHANGE, VARIANT_PACK });

		// An alternative may keep the reference, so the value is offered to the alternatives
		// before its pointee is read out.
		plan("ref i32", "i32 | i64").succeeds().steps({ DEREF, VARIANT_PACK }).packsInto("i32");
		plan("ref i32", "i64 | bool").succeeds().packsInto("i64");
		plan("ref i32", "i64 | ref i32").succeeds().steps({ VARIANT_PACK }).packsInto("ref i32");
		plan("ref i32", "i64 | ref i32 | u64").succeeds().packsInto("ref i32");
		plan("ref (i32 | bool)", "i32 | bool").succeeds().steps({ DEREF });

		// Variants and tuples are packed whole, like any other value.
		plan("i32 | bool", "(i32 | bool) | f64").succeeds().packsInto("Variant (bool, i32)");
		plan("(box i32, i32)", "bool | (box i32, i32)").succeeds().steps({ HAND_OVER, VARIANT_PACK });
		plan("(i32, bool)", "f64 | (i64, bool)")
			.succeeds()
			.steps({ ELEMENTWISE, VARIANT_PACK })
			.part(1)
			.steps({});
		plan("(i32, i32)", "(i32 | bool, i64)").succeeds().part(0).packsInto("i32");

		// An alternative that is a variant ranks with its own pack, wherever it sits.
		plan("u8", "bool | (u8 | f64)").succeeds().packsInto("Variant (f64, u8)");
		plan("char", "bool | (char | f64)").succeeds().packsInto("Variant (char, f64)");
		plan("i32", "(i32 | bool) | i64").succeeds().packsInto("i64");
		plan(type("(i32, i32)"), variantOf({ type("(i32 | bool, i64)"), type("(i64, i64)") }))
			.succeeds()
			.packsInto("Tuple(i64, i64)");

		plan("u8", "u16 | u32").refuses().ambiguousBetween({ 0, 1 });
		plan("u8", "i16 | u16").refuses().ambiguousBetween({ 0, 1 });
		plan("bool", "i32 | u8").refuses().ambiguousBetween({ 0, 1 });
		plan("i8", "i16 | i32 | i64").refuses().ambiguousBetween({ 0, 1, 2 });
		plan("u8", "(u16 | f64) | (u32 | f64)").refuses().ambiguousBetween({ 0, 1 });
		plan("u8", "(u8 | u16) | (u8 | f64)").refuses().ambiguousBetween({ 0, 1 });
		plan("u8", "((u8 | u16) | f64) | (u8 | f64)").refuses().ambiguousBetween({ 0, 1 });
		plan("(u8, u8)", "(u16 | u32, u8)")
			.refuses()
			.because<SubPartRefused>()
			.component(0)
			.ambiguousBetween({ 0, 1 });

		// A rank is only the worst thing done, so widening one component ties with widening two.
		plan(type("(u8, u8)"), variantOf({ type("(u16, u16)"), type("(u8, u16)") }))
			.refuses()
			.ambiguousBetween({ 0, 1 });
		plan(type("(i32, i32)"), variantOf({ type("(i64, i64)"), type("(i32, i64)") }))
			.refuses()
			.ambiguousBetween({ 0, 1 });

		// A value no alternative takes is refused with the reason of every alternative.
		plan("i32", "f32 | f64").refuses().because<IncompatibleTypes>().causes(2);
		plan("f64", "bool | i64").refuses().because<IncompatibleTypes>().causes(2);
		plan("i32", "bool | i8").refuses().because<IncompatibleTypes>().causes(2);
		plan("i32", "bool | ref i32").refuses().because<IncompatibleTypes>().causes(2);
		plan(type("i32"), variantOf({ boolean, mutable_pointer })).refuses().causes(2);
		plan("i32 | bool", "i32 | bool | f64").refuses().because<IncompatibleTypes>().causes(3);
		plan("f64", "i32 | bool")
			.refuses()
			.because<IncompatibleTypes>()
			.alternative(0)
			.because<IncompatibleTypes>();
		plan("(i32, f64)", "bool | (i64, f32)")
			.refuses()
			.because<IncompatibleTypes>()
			.causes(2)
			.alternative(0)
			.because<SubPartRefused>()
			.component(1)
			.about("f64", "f32");
	}

	void typeCoercions() {
		plan("i32", "i32").succeeds().steps({});
		plan("i32", "i64").succeeds().steps({ NUMERIC });
		plan("u32", "i64").succeeds().steps({ NUMERIC });
		plan("u8", "u64").succeeds().steps({ NUMERIC });
		plan("f32", "f64").succeeds().steps({ NUMERIC });
		plan("bool", "i32").succeeds().steps({ NUMERIC });
		plan("char", "bool").succeeds().steps({ ZERO_CHECK });
		plan(byte_type, boolean).succeeds().steps({ ZERO_CHECK });
		plan(mutable_pointer, boolean).succeeds().steps({ ZERO_CHECK });
		plan(mutable_pointer, immutable_pointer)
			.succeeds()
			.doesNothing()
			.steps({ MUTABILITY_CHANGE });
		plan("()", "type").succeeds().steps({ LIFT_TO_TYPE });
		plan("((), ())", "type").succeeds().steps({ LIFT_TO_TYPE });

		// Void has no value, so it is retyped into anything and nothing is handed over.
		plan("void", "void").succeeds().steps({});
		plan("void", "i32").succeeds().steps({ RETYPE_VOID });
		plan("void", "(i32, bool)").succeeds().steps({ RETYPE_VOID });
		plan("void", "i32 | bool").succeeds().steps({ RETYPE_VOID });
		plan("void", "ref i32").succeeds().steps({ RETYPE_VOID });
		plan("void", "box i32").succeeds().steps({ RETYPE_VOID });

		// A value copied into a location of its own keeps no mutability, and a reference is only
		// rebound, while a box is handed over.
		plan(immutable(type("i32")), type("i32")).succeeds().steps({});
		plan("ref i32", "i32").succeeds().steps({ DEREF });
		plan("box i32", "i32").succeeds().steps({ DEREF });
		plan("ref i32", "i64").succeeds().steps({ DEREF, NUMERIC });
		plan("ref i32", "ref i32").succeeds().steps({});
		plan(type("ref i32"), immutable(type("ref i32")))
			.succeeds()
			.doesNothing()
			.steps({ MUTABILITY_CHANGE });
		plan("box i32", "box i32").succeeds().steps({ HAND_OVER });
		plan("Owner", "Owner").succeeds().steps({ HAND_OVER });

		const NodeCheck pair = plan("(i32, i32)", "(i64, i64)").succeeds().steps({ ELEMENTWISE });
		pair.part(0).steps({ NUMERIC });
		pair.part(1).steps({ NUMERIC });
		plan("(i32, (bool, u8))", "(i64, (i32, u16))")
			.succeeds()
			.part(1)
			.steps({ ELEMENTWISE })
			.part(1)
			.steps({ NUMERIC });
		plan("(void, i32)", "(i64, i64)").succeeds().part(0).steps({ RETYPE_VOID });
		plan("ref (i32, i32)", "(i64, i64)").succeeds().steps({ DEREF, ELEMENTWISE });
		plan("(ref i32, i32)", "(i32, i64)").succeeds().part(0).steps({ DEREF });
		const NodeCheck owned = plan("(box i32, i32)", "(box i32, i64)").succeeds();
		owned.part(0).steps({ HAND_OVER });
		owned.part(1).steps({ NUMERIC });
		plan("ref (box i32, i32)", "(box i32, i64)")
			.succeeds()
			.steps({ DEREF, ELEMENTWISE })
			.part(0)
			.steps({ HAND_OVER });

		// A tuple none of whose parts does anything is not taken apart.
		plan("(const i32, i32)", "(i32, i32)").succeeds().doesNothing().steps({ MUTABILITY_CHANGE });
		plan("(box i32, const i32)", "(box i32, i32)")
			.succeeds()
			.steps({ HAND_OVER, MUTABILITY_CHANGE });
	}

	void typeCoercionErrors() {
		using namespace coercion_error;

		plan("i64", "i32").refuses().because<IncompatibleTypes>().aboutTheWholeCoercion();
		plan("i32", "u64").refuses().because<IncompatibleTypes>();
		plan("i32", "f64").refuses().because<IncompatibleTypes>();
		plan("f64", "f32").refuses().because<IncompatibleTypes>();
		plan("bool", "f64").refuses().because<IncompatibleTypes>();
		plan("f64", "bool").refuses().because<IncompatibleTypes>();
		// @TODO: #3631 An integer is not checked against zero implicitly for now.
		plan("i32", "bool").refuses().because<IncompatibleTypes>();
		plan("char", "u32").refuses().because<IncompatibleTypes>();
		plan(byte_type, type("u8")).refuses().because<IncompatibleTypes>();
		plan(immutable_pointer, mutable_pointer).refuses().because<IncompatibleTypes>();
		plan("()", "i32").refuses().because<IncompatibleTypes>();
		plan("(i32, i32)", "type").refuses().because<IncompatibleTypes>();
		plan("i32 | bool", "i32").refuses().because<IncompatibleTypes>();
		plan("i32 | bool", "bool").refuses().because<IncompatibleTypes>();
		plan(functionOf(type("i32"), type("i32")), type("i32"))
			.refuses()
			.because<IncompatibleTypes>();
		plan(functionOf(type("i32"), type("i32")), functionOf(type("i32"), type("i64")))
			.refuses()
			.because<NotImplemented>();

		// A reference is never taken implicitly, and nothing converts behind one.
		plan("i32", "ref i32").refuses().because<IncompatibleTypes>();
		plan("i32", "box i32").refuses().because<IncompatibleTypes>();
		plan("ref i32", "box i32").refuses().because<IncompatibleTypes>();
		plan("box i32", "ref i32").refuses().because<IncompatibleTypes>();
		plan("ref i32", "ref i64").refuses().because<IncompatibleTypes>();
		plan("ref (i32, i32)", "ref (const i32, i32)").refuses().because<IncompatibleTypes>();
		plan(immutable(type("ref i32")), type("ref i32")).refuses().because<MutabilityMismatch>();
		plan("(const box i32, i32)", "(box i32, i32)")
			.refuses()
			.because<SubPartRefused>()
			.component(0)
			.because<MutabilityMismatch>();

		// Every component that refuses is a cause, as deep as the types go.
		plan("(i32, i32)", "(i64, f64)")
			.refuses()
			.because<SubPartRefused>()
			.causes(1)
			.component(1)
			.because<IncompatibleTypes>()
			.about("i32", "f64");
		plan("(i32, i32)", "(bool, f64)").refuses().because<SubPartRefused>().causes(2);
		plan("((i32, i32), i32)", "((i64, f64), i64)")
			.refuses()
			.because<SubPartRefused>()
			.component(0)
			.because<SubPartRefused>()
			.component(1)
			.because<IncompatibleTypes>();
		plan("(i32, i32)", "(i32, i32, i32)").refuses().because<IncompatibleTypes>().causes(0);
	}

	void ownership() {
		using namespace coercion_error;
		using enum PrimaryCategory;

		// A value copied by its bytes has nothing to hand over.
		coerce("x", "i64").succeeds().steps({ NUMERIC });
		coerce("1i32", "i64").succeeds().steps({ NUMERIC });
		coerce("b", "i32").succeeds().steps({ DEREF });
		coerce(ExpressionType<>{ type("void"), ValueCategory(Temporary) }, type("(box i32, i32)"))
			.succeeds()
			.steps({ RETYPE_VOID });

		// A temporary nobody reads again is moved, anything else is the user's to copy or move.
		coerce("makeBox()", "box i32").succeeds().steps({ IMPLICIT_MOVE });
		coerce("makeBoxAndInt()", "(box i32, i32)").succeeds().steps({ IMPLICIT_MOVE });
		coerce("makeOwner()", "Owner").succeeds().steps({ IMPLICIT_MOVE });
		coerce("b", "box i32").refuses().because<RequiresExplicitCopyMove>();
		coerce("t", "(box i32, i32)").refuses().because<RequiresExplicitCopyMove>();
		coerce("o", "Owner").refuses().because<RequiresExplicitCopyMove>();
		coerce("global_box", "box i32").refuses().because<RequiresExplicitCopyMove>();
		coerce("global_pair", "(box i32, i32)").refuses().because<RequiresExplicitCopyMove>();
		coerce("x", "i8").refuses().because<IncompatibleTypes>();

		// A move the user wrote is not the coercion's to add.
		coerce("move t", "(box i32, i32)").succeeds().steps({});
		coerce("move o", "Owner").succeeds().steps({});

		// A reference is only rebound, and what is read out of one is never moved, even out of
		// a box that dies with the expression.
		coerce("r", "ref (box i32, i32)").succeeds().steps({});
		coerce("r", "(box i32, i32)").refuses().because<RequiresExplicitCopyMove>();
		coerce(
			ExpressionType<>{ type("box (box i32, i32)"), ValueCategory(Temporary) },
			type("(box i32, i32)")
		)
			.refuses()
			.because<RequiresExplicitCopyMove>();

		// A value taken apart is handed over part by part, and a part came from wherever the
		// value came from.
		const NodeCheck temporary
			= coerce("makeBoxAndInt()", "(box i32, i64)").succeeds().steps({ ELEMENTWISE });
		temporary.part(0).steps({ IMPLICIT_MOVE });
		temporary.part(1).steps({ NUMERIC });
		coerce("move t", "(box i32, i64)").succeeds().part(0).steps({});
		coerce("t", "(box i32, i64)")
			.refuses()
			.because<SubPartRefused>()
			.component(0)
			.because<RequiresExplicitCopyMove>();
		coerce("global_pair", "(box i32, i64)")
			.refuses()
			.because<SubPartRefused>()
			.component(0)
			.because<RequiresExplicitCopyMove>();

		// A tuple that only changes mutability is not taken apart, so it is handed over whole.
		coerce("makeBoxAndConstInt()", "(box i32, i32)")
			.succeeds()
			.steps({ IMPLICIT_MOVE, MUTABILITY_CHANGE });
		coerce("c", "(box i32, i32)")
			.refuses()
			.because<RequiresExplicitCopyMove>()
			.aboutTheWholeCoercion();

		// A value is handed over right before it is packed, and a refusal names what was asked.
		coerce("makeBoxAndInt()", "bool | (box i32, i32)")
			.succeeds()
			.steps({ IMPLICIT_MOVE, VARIANT_PACK });
		coerce("makeOwner()", "Owner | bool").succeeds().packsInto("Class Owner");
		coerce("t", "bool | (box i32, i32)")
			.refuses()
			.because<RequiresExplicitCopyMove>()
			.aboutTheWholeCoercion();
		coerce("r", "bool | (box i32, i32)")
			.refuses()
			.because<RequiresExplicitCopyMove>()
			.aboutTheWholeCoercion();
	}

	void tupleLiterals() {
		using namespace coercion_error;

		// Moving this literal whole would copy the local box by its bytes and free it twice.
		coerce("(b, 1i64)", "(box i32, i64)")
			.refuses()
			.because<SubPartRefused>()
			.aboutTheWholeCoercion()
			.component(0)
			.because<RequiresExplicitCopyMove>();
		coerce("(b, 1i64)", "bool | (box i32, i64)")
			.refuses()
			.because<SubPartRefused>()
			.aboutTheWholeCoercion()
			.component(0)
			.because<RequiresExplicitCopyMove>();
		coerce("(makeBox(), x, b)", "(box i32, i64, box i32)")
			.refuses()
			.because<SubPartRefused>()
			.causes(1)
			.component(2)
			.because<RequiresExplicitCopyMove>();

		const NodeCheck built
			= coerce("(makeBox(), 1i64)", "(box i32, i64)").succeeds().steps({ ELEMENTWISE });
		built.part(0).steps({ IMPLICIT_MOVE });
		built.part(1).steps({});
		coerce("(makeBox(), 1i64)", "(box i32, const i64)")
			.succeeds()
			.steps({ ELEMENTWISE, MUTABILITY_CHANGE })
			.part(0)
			.steps({ IMPLICIT_MOVE });
		coerce("(move b, 1i64)", "(box i32, i64)").succeeds().part(0).steps({});

		const NodeCheck mixed
			= coerce("(makeBox(), x, move b)", "(box i32, i64, box i32)").succeeds();
		mixed.part(0).steps({ IMPLICIT_MOVE });
		mixed.part(1).steps({ NUMERIC });
		mixed.part(2).steps({});

		const NodeCheck nested = coerce("(1i32, (x, move b))", "(i64, (i64, box i32))").succeeds();
		nested.part(0).steps({ NUMERIC });
		nested.part(1).part(0).steps({ NUMERIC });
		nested.part(1).part(1).steps({});

		coerce("(1i32, x)", "(i32, i32)").succeeds().steps({});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
