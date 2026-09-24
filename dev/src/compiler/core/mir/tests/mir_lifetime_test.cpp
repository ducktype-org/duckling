/**
 * @file mir_lifetime_test.cpp
 * @brief Tests for MIR lifetime analysis and validation
 */

#include "utils/lifetime_checker.hpp"
#include "utils/test_utils.hpp"

#include <ctv/ctv.hpp>
#include <driver/test_utils.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using namespace compiler::mir::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRLifetimeTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRLifetimeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(lifetimeAnalysisTest);
		TESTER_ADD_TEST(moveValidationTest);
		TESTER_ADD_TEST(reinitAfterMoveTest);
		TESTER_ADD_TEST(moveDestructorTest);
		TESTER_ADD_TEST(variantOwnershipLifetimeTest);
		TESTER_ADD_TEST(lifetimeFlagsTest);
		TESTER_ADD_TEST(moveOwnershipTest);
		TESTER_ADD_TEST(simpleLifetimeSequenceTest);
		TESTER_ADD_TEST(lifetimeFlagsRepeatedBlocks);
		TESTER_ADD_TEST(lifetimeFlagsSingleBlock);
		TESTER_ADD_TEST(lifetimeFlagsNestedBlocks);
		TESTER_ADD_TEST(destructorInsertionTest);
		TESTER_ADD_TEST(intermediateDestructorBlocksTest);
		TESTER_ADD_TEST(conditionalExpressionTemporariesTest);
	}

protected:
	/**
	 * Some of the modules below hold a `box`, whose allocation and destruction go through the
	 * `boxAlloc`/`boxFree` primitives of `core.containers`, so the standard library has to be
	 * loaded. The modules themselves are still built as standalone trees by `getModule`.
	 */
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		auto         init_result
			= compiler::driver::test_utils::initializeCompilerForTests({}, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void lifetimeAnalysisTest() {
		// Basic test that lifetime analysis runs without throwing exceptions
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		auto foo_mir = getMIRFunctionByName(module, "foo");

		ASSERT_EQUAL(foo_mir->local_list.size(), 5);

		// Verify lifetime scopes are assigned
		for (const auto& local: foo_mir->local_list) ASSERT_HAS_VALUE(local.scope);
	}

	void simpleLifetimeSequenceTest() {
		auto [module, scope] = getModule(fs::File(path("modules/simple_lifetime_sequence")));

		auto goo_mir = getMIRFunctionByName(module, "goo");

		LifetimeChecker checker;
		checker.expectConstruct("a")
			.expectInstruction(compiler::mir::Operation::Call)
			.expectDestruct("a")
			.validate(goo_mir);
	}

	void lifetimeFlagsRepeatedBlocks() {
		auto [module, scope]
			= getModule(fs::File(path("modules/lifetime_flags/repeated_blocks.dk")));
		auto            foo_mir = getMIRFunctionByName(module, "function");
		LifetimeChecker checker;
		checker.expectConstruct("a")
			.expectScopeStart("a")

			.expectConstruct("b")
			.expectScopeStart("b")
			.expectDestruct("b")
			.expectScopeEnd("b")

			.expectConstruct("c")
			.expectScopeStart("c")
			.expectDestruct("c")
			.expectScopeEnd("c")

			.expectScopeStart("5.tmp")  // The if condition temporary
			.expectInstruction(compiler::mir::Operation::Branch)
			.expectScopeEnd("5.tmp")
			.expectDestruct("a")
			.expectInstruction(compiler::mir::Operation::ReturnValue)
			.expectScopeEnd("a")
			.expectMove("a")
			.expectInstruction(compiler::mir::Operation::ReturnValue)
			.expectScopeEnd("a")
			.validate(foo_mir);

		auto count_with_flag = [&](compiler::mir::LifetimeFlag flag) {
			usize count = 0;
			for (const auto& local: foo_mir->local_list)
				if (local.lifetime_flags.contains(flag)) count++;
			return count;
		};

		// One per `return` statement.
		ASSERT_EQUAL_PRINT(2, count_with_flag(compiler::mir::LifetimeFlag::ReturnTmpValue));
		// One for the single `if` condition.
		ASSERT_EQUAL_PRINT(1, count_with_flag(compiler::mir::LifetimeFlag::ConditionTmpValue));
	}

	void lifetimeFlagsSingleBlock() {
		auto [module, scope] = getModule(fs::File(path("modules/lifetime_flags/single_block.dk")));
		auto            main = getMIRFunctionByName(module, "main");
		LifetimeChecker checker;
		checker.expectConstruct("x")
			.expectScopeStart("x")
			.expectScopeStart("y")
			.expectScopeStart("z")

			.expectConstruct("y")
			.expectConstruct("z")

			.expectDestruct("z")
			.expectDestruct("y")
			.expectDestruct("x")

			.expectScopeEnd("z")
			.expectScopeEnd("y")
			.expectScopeEnd("x")
			.validate(main);
	}

	void lifetimeFlagsNestedBlocks() {
		auto [module, scope] = getModule(fs::File(path("modules/lifetime_flags/nested_blocks.dk")));
		auto            main = getMIRFunctionByName(module, "main");
		LifetimeChecker checker;
		checker.expectConstruct("x")
			.expectScopeStart("x")

			.expectConstruct("y")
			.expectScopeStart("y")

			.expectConstruct("z")
			.expectScopeStart("z")

			.expectDestruct("z")
			.expectScopeEnd("z")

			.expectDestruct("y")
			.expectScopeEnd("y")

			.expectDestruct("x")
			.expectScopeEnd("x")
			.validate(main);
	}

	void moveValidationTest() {
		// Use-after-move validation, driven by real `move` expressions. `good*` functions lower
		// successfully (and we check the move/destructor shape they produce), while `bad*`
		// functions fail to lower because a moved (or possibly-moved) value is read.
		using compiler::mir::Operation;
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		// In all `good*` functions: Local(0) = param `c`, Local(1) = `a`, Local(2) = `b`.

		// good1: straight-line move. `b` is moved (so it has no destructor), while `a` stays alive
		// to its scope end and gets an unconditional `Destruct`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good1"));

		// good2: `b` is moved only on the `if` branch, so at the merge it is `MaybeMoved` and gets
		// a conditional `DestructIf`; `a` still gets an unconditional `Destruct`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::DestructIf)
			.expectDestruct("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good2"));

		// good3: `b` is loop-local and moved on a conditional path inside the loop body, so it is
		// `MaybeMoved` at the end of the body -> `DestructIf`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::DestructIf)
			.expectDestruct("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good3"));

		// good4: `b` is loop-local and moved unconditionally in the body, so it is `Moved` at the
		// end of the body and gets no destructor at all; only `a` is destructed.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good4"));

		// `bad*` functions read a moved / possibly-moved value, so lowering must fail.
		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			auto check_fails = [&](std::string_view name) {
				for (CRef<compiler::helios::HOUTFunction> fun: unit.functions) {
					if (fun->declaration->original_name.strView() != name) continue;
					assertTrue(
						ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->hasFailed(),
						base::strConcat(name, " should fail to lower")
					);
					return;
				}
				CORE_PANIC(base::strConcat("Function '", name, "' not found in module"));
			};

			check_fails("bad1");
			check_fails("bad2");
			check_fails("bad3");
		});
	}

	void reinitAfterMoveTest() {
		// Reinitialization by assignment. A bare-local store (e.g. `b = 99`) carries a `Reinit`
		// flag that revives the local for move state, so reading it after a prior move-out is
		// valid. Each `getMIRFunctionByName` below panics if lowering fails, so the mere fact these
		// `reinit*` functions lower is the regression guard against the false use-after-move.
		using compiler::mir::Operation;
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		// reinit1: `b` moved out (`a = move b`), reinitialized (`b = 99`), then read (`eat(b)`).
		// The `a = move b` store also reinitializes `a`.
		auto reinit1 = getMIRFunctionByName(module, "reinit1");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")                                    // a = move b
			.expectReinit("b")                                  // b = 99
			.validate(reinit1);
		LifetimeChecker{}.expectReinit("a").validate(reinit1);  // a = move b

		// reinit2: `a` is moved and reinitialized on both branches, alive at the merge read.
		auto reinit2 = getMIRFunctionByName(module, "reinit2");
		LifetimeChecker{}.expectMove("a").expectReinit("a").validate(reinit2);

		// reinit3: `a` is moved then reinitialized inside the loop body.
		auto reinit3 = getMIRFunctionByName(module, "reinit3");
		LifetimeChecker{}.expectMove("a").expectReinit("a").validate(reinit3);
	}

	void moveDestructorTest() {
		// Move-aware destructor insertion. With the move-state analysis in place, a local that is
		// definitely alive at its scope end gets an unconditional `Destruct`, while a local that
		// is moved on only some control-flow paths is `MaybeMoved` and gets a conditional
		// `DestructIf`. A definitely-moved local gets no destructor at all.
		auto [module, scope] = getModule(fs::File(path("modules/move_lifetime")));

		// `a` is never moved -> alive at scope end -> unconditional `Destruct`.
		auto no_move = getMIRFunctionByName(module, "noMove");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectInstruction(compiler::mir::Operation::Destruct)
			.expectDestruct("a")
			.validate(no_move);

		// `a` is moved on the `if` branch only -> `MaybeMoved` at the merge -> `DestructIf`.
		auto maybe_move = getMIRFunctionByName(module, "maybeMove");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectMove("a")
			.expectInstruction(compiler::mir::Operation::DestructIf)
			.expectDestruct("a")
			.validate(maybe_move);
	}

	void variantOwnershipLifetimeTest() {
		// A variant owns its active payload, so moving a value into one and matching it out
		// again has to hand that ownership over each time - exactly one destruction in total,
		// and never one of the source the value was moved out of.
		using compiler::mir::Operation;
		auto [module, scope] = getModule(fs::File(path("modules/variant_lifetime")));

		auto count_flag_on = [this](
								 CRef<compiler::mir::Function>      func,
								 compiler::mir::OperationFlag::Flag flag,
								 std::string_view                   var_name
							 ) {
			usize count = 0;
			for (const auto* instr: allInstructions(func))
				for (const auto& instr_flag: instr->flags)
					if (instr_flag.flag == flag && instr_flag.local->getName().strView() == var_name)
						count++;
			return count;
		};

		auto count_destructions = [this](CRef<compiler::mir::Function> func) {
			usize count = 0;
			for (const auto* instr: allInstructions(func))
				if (instr->operation == compiler::mir::Operation::Destruct
				    || instr->operation == compiler::mir::Operation::DestructIf)
					count++;
			return count;
		};

		// The `VariantConstruct` moves the local into the variant, so `payload` is `Moved` and
		// gets no destructor, while the variant is destroyed at its scope end.
		auto move_into = getMIRFunctionByName(module, "moveIntoVariant");
		LifetimeChecker{}
			.expectConstruct("payload")
			.expectScopeStart("payload")
			.expectScopeStart("v")
			.expectMove("payload")
			.expectConstruct("v")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("v")
			.expectScopeEnd("v")
			.expectScopeEnd("payload")
			.validate(move_into);
		ASSERT_EQUAL_PRINT(
			0, count_flag_on(move_into, compiler::mir::OperationFlag::Flag::Destruct, "payload")
		);
		ASSERT_EQUAL_PRINT(1, count_destructions(move_into));

		// The match takes the variant over - `v` is `Moved` into the subject temporary, which
		// carries `NoDestructor` - and the case binding is what takes the payload and destroys
		// it, at the end of its own case scope.
		auto match_out = getMIRFunctionByName(module, "matchOutOfVariant");
		LifetimeChecker{}
			.expectMove("payload")
			.expectConstruct("v")
			.expectMove("v")
			.expectConstruct("r")
			.expectScopeStart("r")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("r")
			.expectScopeEnd("r")
			.validate(match_out);
		for (std::string_view moved_from: { "payload", "v" })
			ASSERT_EQUAL_PRINT(
				0, count_flag_on(match_out, compiler::mir::OperationFlag::Flag::Destruct, moved_from)
			);
		// The `i32` case binds a trivially destructible payload, so nothing is destroyed there.
		ASSERT_EQUAL_PRINT(
			0, count_flag_on(match_out, compiler::mir::OperationFlag::Flag::Destruct, "n")
		);
		// The binding is the only owner at the end, so the payload is destroyed exactly once -
		// the subject temporary the variant was moved into must not destroy it as well.
		ASSERT_EQUAL_PRINT(1, count_destructions(match_out));

		// A constrained wildcard names no binding, so the payload lands in a temporary the match
		// itself destroys - `v` is still never destroyed twice.
		auto drop_in_match = getMIRFunctionByName(module, "dropInMatch");
		LifetimeChecker{}
			.expectMove("payload")
			.expectConstruct("v")
			.expectMove("v")
			.expectInstruction(Operation::Destruct)
			.validate(drop_in_match);
		for (std::string_view moved_from: { "payload", "v" })
			ASSERT_EQUAL_PRINT(
				0,
				count_flag_on(
					drop_in_match, compiler::mir::OperationFlag::Flag::Destruct, moved_from
				)
			);
		ASSERT_EQUAL_PRINT(1, count_destructions(drop_in_match));
	}

	/**
	 * @brief Finds in @p func the single conditional destruction of the local named @p var_name,
	 * which is the `DestructIf` reading the lifetime flag of that local.
	 */
	const compiler::mir::Instruction* conditionalDropOf(
		CRef<compiler::mir::Function> func, std::string_view var_name
	) {
		using namespace compiler::mir;

		const Instruction* found = nullptr;
		for (const auto* instr: allInstructions(func)) {
			if (instr->operation != Operation::DestructIf) continue;

			const auto& dropped = instr->arguments.at(1).get<MIRPlace>();
			if (dropped.getBase<MIRLocalRef>()->getName().strView() != var_name) continue;

			ASSERT_TRUE(found == nullptr);
			found = instr;
		}
		ASSERT_TRUE(found != nullptr);
		return found;
	}

	/**
	 * @brief The values written to @p flag over the whole function in block order, as a string of
	 * `T` (set) and `F` (cleared).
	 */
	std::string lifetimeFlagWrites(
		CRef<compiler::mir::Function> func, compiler::mir::MIRLocalRef flag
	) {
		using namespace compiler::mir;

		std::string writes;
		for (const auto* instr: allInstructions(func)) {
			if (instr->operation != Operation::Assign) continue;
			if (not instr->output.has_value() or not instr->output->isLocal()) continue;
			if (instr->output->getBase<MIRLocalRef>() != flag) continue;

			auto value = instr->arguments.at(0).get<MIRConstant>().value.get<bool>();
			ASSERT_HAS_VALUE(value);
			writes += value.value() ? 'T' : 'F';
		}
		return writes;
	}

	void lifetimeFlagsTest() {
		using namespace compiler::mir;
		auto [module, scope] = getModule(fs::File(path("modules/move_lifetime")));

		{
			// The flag is cleared when the scope of `a` starts, set when `a` is constructed, the
			// `if` branch moves `a` and clears the flag, and the drop at the scope end reads it.
			auto maybe_move = getMIRFunctionByName(module, "maybeMove");
			LifetimeChecker{}
				.expectConstruct("a")
				.expectInstruction(Operation::Assign)
				.expectMove("a")
				.expectInstruction(Operation::Assign)
				.expectInstruction(Operation::DestructIf)
				.validate(maybe_move);

			auto drop = conditionalDropOf(maybe_move, "a");
			auto flag = drop->arguments.at(2).get<MIRPlace>().getBase<MIRLocalRef>();
			ASSERT_EQUAL_PRINT(std::string("FTF"), lifetimeFlagWrites(maybe_move, flag));

			auto dropped = drop->arguments.at(1).get<MIRPlace>().getBase<MIRLocalRef>();
			ASSERT_EQUAL(compiler::tsh::Kind::Bool, flag->type.getType().getKind());
			// The flag shares the lifetime scope of the local it tracks, so it gets the same
			// `ScopeStart` / `ScopeEnd` placement.
			ASSERT_TRUE(flag->scope.value() == dropped->scope.value());
			ASSERT_TRUE(flag->lifetime_flags.contains(LifetimeFlag::NoDestructor));
		}
		{
			// A parameter owns its value on entry, its flag is set by the very first instruction of
			// the entry block instead.
			auto  param       = getMIRFunctionByName(module, "maybeMoveParam");
			auto  param_flag  = conditionalDropOf(param, "a")->arguments.at(2).get<MIRPlace>();
			auto& entry_block = *param->blocks.at(param->block_order.front());
			auto& first_instr = entry_block.instructions.at(0);

			ASSERT_EQUAL(Operation::Assign, first_instr.operation);
			ASSERT_TRUE(
				first_instr.output->getBase<MIRLocalRef>() == param_flag.getBase<MIRLocalRef>()
			);
			LifetimeChecker{}
				.expectMove("a")
				.expectInstruction(Operation::Assign)
				.expectInstruction(Operation::DestructIf)
				.validate(param);
			ASSERT_EQUAL_PRINT(
				std::string("TF"), lifetimeFlagWrites(param, param_flag.getBase<MIRLocalRef>())
			);
		}
		{
			// A local that is never destructed conditionally doesn't have a lifetime flag.
			auto no_move = getMIRFunctionByName(module, "noMove");
			for (const auto* instr: allInstructions(no_move))
				ASSERT_TRUE(instr->operation != Operation::DestructIf);
			ASSERT_EQUAL_PRINT(no_move->next_local_id, static_cast<u64>(no_move->local_list.size()));
		}
	}

	/**
	 * @brief A temporary built inside a conditionally evaluated part of an expression lives in the
	 * scope of the whole expression, so it is destructed where that expression ends, conditionally.
	 *
	 * The three forms that evaluate a part of an expression conditionally are a lazy `or` / `and`,
	 * a ternary and a match. In all of them the temporary of the branch that builds one is
	 * initialized on one path only, so it gets a single `DestructIf` at the end of the expression,
	 * guarded by a lifetime flag that is cleared where its scope starts and set right after the
	 * temporary is constructed.
	 */
	void conditionalExpressionTemporariesTest() {
		using namespace compiler::mir;
		namespace test_utils = compiler::mir::test_utils;

		test_utils::checkLoweredModule(
			R"(class Res {
                   id: i32 = 0;
                   Res.destroy() = { var t: i32 = id; }
               }
               fun make(id: i32) -> Res = Res(id);
               fun lazyOr(c: bool) -> bool = { return c or make(1).id == 0; }
               fun ternary(c: bool) -> i32 = { return if c then make(2).id else 0; }
               fun matchExpr(v: i32 | f32) -> i32 = {
                   return match (v) {
                       case x : i32 = make(3).id;
                       case _ = 0;
                   };
               })",
			[this](query::Context&, const MIRUnit& unit) {
				for (const std::string_view name: { "lazyOr", "ternary", "matchExpr" }) {
					auto function = test_utils::functionOfUnit(unit, name);

					// The temporary is only maybe-initialized at the end of the expression, so it
				    // is never destructed unconditionally.
					assertEqual(
						0u,
						countOperation(function, Operation::Destruct),
						base::strConcat("Unconditional destructor in `", name, "`")
					);
					assertEqual(
						1u,
						countOperation(function, Operation::DestructIf),
						base::strConcat("Expected a single conditional destructor in `", name, "`")
					);

					// The `Res` temporary is the only local that owns a value here, so it is the
				    // only one whose lifetime ends.
					const auto* drop
						= onlyInstructionWithOperation(function, Operation::DestructIf);
					auto dropped = drop->arguments.at(1).get<MIRPlace>().getBase<MIRLocalRef>();
					assertEqual(
						1u,
						countFlag(
							function, OperationFlag::Flag::Destruct, dropped->getName().strView()
						),
						base::strConcat("Expected one ended lifetime in `", name, "`")
					);

					// The flag is cleared where the scope of the temporary starts and set once the
				    // temporary has been constructed, so exactly the path that built it destructs
				    // it.
					auto flag = drop->arguments.at(2).get<MIRPlace>().getBase<MIRLocalRef>();
					assertEqual(
						std::string("FT"),
						lifetimeFlagWrites(function, flag),
						base::strConcat("Unexpected lifetime flag writes in `", name, "`")
					);
					ASSERT_TRUE(flag->scope.value() == dropped->scope.value());
				}
			}
		);
	}

	/**
	 * @brief The only instruction of the function using the given operation.
	 */
	const compiler::mir::Instruction* onlyInstructionWithOperation(
		CRef<compiler::mir::Function> func, compiler::mir::Operation operation
	) {
		const compiler::mir::Instruction* found = nullptr;
		for (const auto* instr: allInstructions(func))
			if (instr->operation == operation) {
				ASSERT_TRUE(found == nullptr);
				found = instr;
			}
		ASSERT_TRUE(found != nullptr);
		return found;
	}

	/**
	 * @brief Every instruction of the function in block order.
	 */
	std::vector<const compiler::mir::Instruction*> allInstructions(CRef<compiler::mir::Function> func
	) {
		std::vector<const compiler::mir::Instruction*> instructions;
		for (const auto& block_id: func->block_order) {
			const auto& block = func->blocks.at(block_id);
			for (const auto& instr: block->instructions) instructions.push_back(&instr);
			instructions.push_back(&block->terminator);
		}
		return instructions;
	}

	/**
	 * @brief How many times a given flag is set for the local.
	 */
	usize countFlag(
		CRef<compiler::mir::Function>      func,
		compiler::mir::OperationFlag::Flag flag,
		std::string_view                   var_name
	) {
		usize count = 0;
		for (const auto* instr: allInstructions(func))
			for (const auto& set: instr->flags)
				if (set.flag == flag && set.local->getName().strView() == var_name) count++;
		return count;
	}

	/**
	 * @brief The single instruction setting a given flag for the local.
	 */
	const compiler::mir::Instruction* onlyInstructionWithFlag(
		CRef<compiler::mir::Function>      func,
		compiler::mir::OperationFlag::Flag flag,
		std::string_view                   var_name
	) {
		const compiler::mir::Instruction* found = nullptr;
		for (const auto* instr: allInstructions(func))
			for (const auto& set: instr->flags)
				if (set.flag == flag && set.local->getName().strView() == var_name) {
					ASSERT_TRUE(found == nullptr);
					found = instr;
				}
		ASSERT_TRUE(found != nullptr);
		return found;
	}

	bool hasFlagFor(
		const compiler::mir::Instruction&  instr,
		compiler::mir::OperationFlag::Flag flag,
		std::string_view                   var_name
	) {
		return std::ranges::any_of(instr.flags, [&](const auto& set) {
			return set.flag == flag && set.local->getName().strView() == var_name;
		});
	}

	/**
	 * @brief Asserts that the value moved out of `var_name` ends up in a temporary that owns it
	 * and destructs it.
	 */
	void assertMovedIntoDestructedTemporary(
		CRef<compiler::mir::Function> func, std::string_view var_name
	) {
		using Flag = compiler::mir::OperationFlag::Flag;

		const auto* move_instr = onlyInstructionWithFlag(func, Flag::Move, var_name);
		ASSERT_TRUE(move_instr->operation == compiler::mir::Operation::Assign);

		// The source is moved out, so it must not be destructed.
		ASSERT_EQUAL_PRINT(0, countFlag(func, Flag::Destruct, var_name));

		usize owners = 0;
		for (const auto& set: move_instr->flags) {
			if (set.flag != Flag::Construct) continue;
			owners++;
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, set.local->getName().strView()));
		}
		ASSERT_EQUAL_PRINT(1, owners);
	}

	void moveOwnershipTest() {
		using Flag = compiler::mir::OperationFlag::Flag;

		auto [module, scope] = getModule(fs::File(path("modules/move_ownership")));

		// The `Call` reading `a` marks it as moved out. No `Assign` into a temporary is built for
		// the move, `a` is not destructed here any more.
		{
			auto        func       = getMIRFunctionByName(module, "handedToCall");
			const auto* move_instr = onlyInstructionWithFlag(func, Flag::Move, "a");
			ASSERT_TRUE(move_instr->operation == compiler::mir::Operation::Call);
			ASSERT_EQUAL_PRINT(0, countFlag(func, Flag::Destruct, "a"));
		}

		// The initialised variable becomes the owner, so the store into it carries the move and
		// `b` is the one that gets destructed.
		{
			auto        func       = getMIRFunctionByName(module, "storedInPlace");
			const auto* move_instr = onlyInstructionWithFlag(func, Flag::Move, "a");
			ASSERT_TRUE(move_instr->operation == compiler::mir::Operation::Assign);
			ASSERT_TRUE(hasFlagFor(*move_instr, Flag::Construct, "b"));
			ASSERT_EQUAL_PRINT(0, countFlag(func, Flag::Destruct, "a"));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "b"));
		}

		// `move a;` hands the value to nobody, so a temporary owns it and destructs it.
		assertMovedIntoDestructedTemporary(getMIRFunctionByName(module, "discarded"), "a");

		// Reading the pointee out of the moved box does not hand the box over.
		assertMovedIntoDestructedTemporary(getMIRFunctionByName(module, "readThrough"), "a");
		assertMovedIntoDestructedTemporary(
			getMIRFunctionByName(module, "readThroughImplicitly"), "a"
		);

		// An implicitly moved argument goes straight to the callee.
		{
			auto                              func = getMIRFunctionByName(module, "movedTemporary");
			usize                             moves      = 0;
			const compiler::mir::Instruction* move_instr = nullptr;
			std::string                       moved_name;
			for (const auto* instr: allInstructions(func))
				for (const auto& set: instr->flags)
					if (set.flag == Flag::Move) {
						moves++;
						move_instr = instr;
						moved_name = std::string(set.local->getName().strView());
					}
			ASSERT_EQUAL_PRINT(1, moves);
			ASSERT_TRUE(move_instr->operation == compiler::mir::Operation::Call);
			ASSERT_EQUAL_PRINT(0, countFlag(func, Flag::Destruct, moved_name));
		}

		// A moved rvalue has no place to be moved out of, so the call writes its result straight
		// into `b`.
		{
			auto func = getMIRFunctionByName(module, "initFromCall");
			ASSERT_EQUAL_PRINT(1, func->local_list.size());

			const auto* construct = onlyInstructionWithFlag(func, Flag::Construct, "b");
			ASSERT_TRUE(construct->operation == compiler::mir::Operation::Call);
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "b"));

			for (const auto* instr: allInstructions(func))
				for (const auto& set: instr->flags) ASSERT_TRUE(set.flag != Flag::Move);
		}
	}

	/**
	 * @brief The ctor and the dtor MIR functions of the named global variable.
	 */
	compiler::mir::MIRCtorDtorPair getGlobalCtorDtor(
		compiler::frontend::ModuleID module_id, std::string_view global_name
	) {
		auto result = query::utils::withContextCompute([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			for (const auto& global: unit.glob_data) {
				if (global->original_name.strView() != global_name) continue;

				const auto& global_data = ctx.query<compiler::mir::LowerGlobalData>(
												 compiler::mir::KeyOf_LowerGlobalData{ global }
				)
				                              ->valueOrPanic();
				return std::get<compiler::mir::MIRCtorDtorPair>(global_data.initial_value);
			}
			CORE_PANIC(base::strConcat("Global with name '", global_name, "' not found in module"));
		});
		return std::any_cast<compiler::mir::MIRCtorDtorPair>(result);
	}

	/**
	 * @brief How many instructions of the function use the given operation.
	 */
	usize countOperation(CRef<compiler::mir::Function> func, compiler::mir::Operation operation) {
		usize count = 0;
		for (const auto* instr: allInstructions(func))
			if (instr->operation == operation) count++;
		return count;
	}

	/**
	 * @brief Whether the instruction calls a function of the given name. Both `Call` and `Destruct`
	 * take the callee as their first argument.
	 */
	bool callsFunction(const compiler::mir::Instruction& instr, std::string_view function_name) {
		if (instr.arguments.empty()) return false;
		const auto* callee
			= std::get_if<compiler::mir::MIRFunctionLiteral>(&instr.arguments.at(0).getVariant());
		return callee != nullptr
		    && compiler::helios::name(callee->helios_id).strView() == function_name;
	}

	/**
	 * @brief Index of the only instruction matching the predicate, within `allInstructions`.
	 */
	usize onlyIndexOf(
		CRef<compiler::mir::Function>                                 func,
		const std::function<bool(const compiler::mir::Instruction&)>& matches
	) {
		base::Optional<usize> found;
		const auto            instructions = allInstructions(func);
		for (usize i = 0; i < instructions.size(); i++)
			if (matches(*instructions.at(i))) {
				ASSERT_TRUE(found.empty());
				found = i;
			}
		ASSERT_HAS_VALUE(found);
		return found.value();
	}

	/**
	 * @brief Destructor insertion in all the places it happens: before an assignment overwriting a
	 * live value, at the end of a scope, and in the dtor of a global variable. Also checks that the
	 * ctor of a global variable never destructs anything, since it initializes raw storage.
	 */
	void destructorInsertionTest() {
		auto [module, scope] = getModule(fs::File(path("modules/destructor_insertion")));

		using enum compiler::mir::Operation;
		using Flag = compiler::mir::OperationFlag::Flag;

		auto is_destruct_of = [&](std::string_view local_name) {
			return [=](const compiler::mir::Instruction& instr) {
				return instr.operation == Destruct && instr.arguments.size() > 1
				    && instr.arguments.at(1).isLocal()
				    && instr.arguments.at(1)
				               .get<compiler::mir::MIRPlace>()
				               .getBase<compiler::mir::MIRLocalRef>()
				               ->getName()
				               .strView()
				           == local_name;
			};
		};

		{  // The overwritten value is destructed before the assignment, and once more at the end of
		   // the scope. Both destructors call the class' `__destruct`.
			auto func = getMIRFunctionByName(module, "assignOverExisting");

			ASSERT_EQUAL_PRINT(2, countOperation(func, Destruct));
			for (const auto* instr: allInstructions(func))
				if (instr->operation == Destruct) ASSERT_TRUE(callsFunction(*instr, "__destruct"));

			// Only the destructor at the end of the scope ends `a`'s lifetime, the one before the
			// assignment is followed by the value being reinitialized.
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "a"));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Reinit, "a"));

			const auto reinit_index
				= onlyIndexOf(func, [&](const compiler::mir::Instruction& instr) {
					  return hasFlagFor(instr, Flag::Reinit, "a");
				  });
			const auto scope_end_destruct_index
				= onlyIndexOf(func, [&](const compiler::mir::Instruction& instr) {
					  return hasFlagFor(instr, Flag::Destruct, "a");
				  });

			// destruct the old value -> assign the new one -> destruct it at the scope end
			const auto instructions         = allInstructions(func);
			usize      first_destruct_index = 0;
			while (!is_destruct_of("a")(*instructions.at(first_destruct_index)))
				first_destruct_index++;

			ASSERT_TRUE(first_destruct_index < reinit_index);
			ASSERT_TRUE(reinit_index < scope_end_destruct_index);
		}

		{  // The new value is computed by a call reading `a` through a reference, so the call must
		   // not write into `a`: its result goes into a temporary, `a` is destructed only after the
		   // call returns, and the assignment overwrites it afterwards.
			auto func = getMIRFunctionByName(module, "assignFromCallTakingRef");

			// The destructor of the overwritten value and the one at the end of the scope.
			ASSERT_EQUAL_PRINT(2, countOperation(func, Destruct));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "a"));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Reinit, "a"));

			// take the address of `a` -> call -> destruct the old value -> overwrite `a`
			LifetimeChecker{}
				.expectConstruct("a")
				.expectInstruction(AddressOf)
				.expectInstruction(Call)
				.expectInstruction(Destruct)
				.expectReinit("a")
				.validate(func);
		}

		{  // Self-assignment reads the value it overwrites, so the copy into the temporary has to
		   // happen before the destructor of the old value runs.
			auto func = getMIRFunctionByName(module, "assignItself");

			ASSERT_EQUAL_PRINT(2, countOperation(func, Destruct));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "a"));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Reinit, "a"));

			// copy `a` into a temporary -> destruct the old value -> assign the temporary back
			LifetimeChecker{}
				.expectConstruct("a")
				.expectInstruction(Assign)
				.expectInstruction(Destruct)
				.expectReinit("a")
				.validate(func);
		}

		{  // A local of a non-trivially destructible type is destructed at the end of its scope,
		   // the inner one before the rest of the function runs. A trivially destructible local is
		   // never destructed.
			auto func = getMIRFunctionByName(module, "scopeEnd");

			ASSERT_EQUAL_PRINT(2, countOperation(func, Destruct));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "a"));
			ASSERT_EQUAL_PRINT(1, countFlag(func, Flag::Destruct, "b"));
			ASSERT_EQUAL_PRINT(0, countFlag(func, Flag::Destruct, "t"));

			// `a` dies at the end of the inner scope, i.e. before `b` is even constructed.
			const auto a_destruct_index = onlyIndexOf(func, is_destruct_of("a"));
			const auto b_construct_index
				= onlyIndexOf(func, [&](const compiler::mir::Instruction& instr) {
					  return hasFlagFor(instr, Flag::Construct, "b");
				  });
			const auto b_destruct_index = onlyIndexOf(func, is_destruct_of("b"));

			ASSERT_TRUE(a_destruct_index < b_construct_index);
			ASSERT_TRUE(b_construct_index < b_destruct_index);
		}

		{  // A global of a class type: the ctor runs the class constructor and moves the result
		   // into the global's storage, without destructing anything - the storage starts
		   // uninitialized. The dtor destructs the global.
			auto [ctor, dtor] = getGlobalCtorDtor(module, "g_res");
			ASSERT_HAS_VALUE(dtor);

			ASSERT_EQUAL_PRINT(0, countOperation(ctor, Destruct));
			ASSERT_EQUAL_PRINT(0, countFlag(ctor, Flag::Destruct, "g_res"));
			ASSERT_TRUE(std::ranges::any_of(allInstructions(ctor), [&](const auto* instr) {
				return instr->operation == Call && callsFunction(*instr, "Res");
			}));
			ASSERT_TRUE(std::ranges::any_of(allInstructions(ctor), [&](const auto* instr) {
				return instr->operation == Call && callsFunction(*instr, "move_in");
			}));

			ASSERT_TRUE(std::ranges::any_of(allInstructions(dtor.value()), [&](const auto* instr) {
				return instr->operation == Call && callsFunction(*instr, "__destruct");
			}));
		}

		{  // A global box: allocated by the ctor, freed by the dtor.
			auto [ctor, dtor] = getGlobalCtorDtor(module, "g_box");
			ASSERT_HAS_VALUE(dtor);

			ASSERT_EQUAL_PRINT(0, countOperation(ctor, Destruct));
			ASSERT_TRUE(std::ranges::any_of(allInstructions(ctor), [&](const auto* instr) {
				return instr->operation == Call && callsFunction(*instr, "boxAlloc");
			}));
			ASSERT_TRUE(std::ranges::any_of(allInstructions(dtor.value()), [&](const auto* instr) {
				return instr->operation == Call && callsFunction(*instr, "boxFree");
			}));
		}

		{  // A trivially destructible global has nothing to destroy, so it gets no dtor at all.
			auto [ctor, dtor] = getGlobalCtorDtor(module, "g_int");

			ASSERT_EQUAL_PRINT(0, countOperation(ctor, Destruct));
			ASSERT_TRUE(dtor.empty());
		}
	}

	/**
	 * @brief Blocks the destructor insertion added for a single outgoing edge - they hold only
	 * destructors and jump to the original destination.
	 */
	usize countIntermediateDestructorBlocks(CRef<compiler::mir::Function> func) {
		usize intermediate_blocks_count = 0;
		for (const auto& block_id: func->block_order) {
			const auto& block = func->blocks.at(block_id);
			if (!block->instructions.empty()
			    && block->instructions[0].operation == compiler::mir::Operation::Nop
			    && block->terminator.operation == compiler::mir::Operation::Jump)
				intermediate_blocks_count++;
		}
		return intermediate_blocks_count;
	}

	/**
	 * @brief When the successors of a terminator end different scopes, the path-specific
	 * destructors go into intermediate blocks. When all the outgoing edges need the same
	 * destructors, they are appended to the current block instead.
	 */
	void intermediateDestructorBlocksTest() {
		auto [module, scope] = getModule(fs::File(path("modules/intermediate_destructor_blocks")));

		// A single loop, so its back edge and its exit edge need different destructors.
		ASSERT_EQUAL_PRINT(2, countIntermediateDestructorBlocks(getMIRFunctionByName(module, "A")));
		// Two nested loops, so both of them need their own intermediate blocks.
		ASSERT_EQUAL_PRINT(4, countIntermediateDestructorBlocks(getMIRFunctionByName(module, "B")));
		// Both `if` branches end the same scope, so no intermediate block is needed.
		ASSERT_EQUAL_PRINT(0, countIntermediateDestructorBlocks(getMIRFunctionByName(module, "C")));
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
