/**
 * @file mir_tests.cpp
 */

#include "utils/test_utils.hpp"

#include <ctv/ctv.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_lowering/mir_lifetimes.hpp>
#include <mir/mir_lowering/mir_liveness.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <diagnostic/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <array>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(simpleVarTest);
		TESTER_ADD_TEST(testTerminatorSuccessors);
		TESTER_ADD_TEST(controlFlowTargetsTest);
		TESTER_ADD_TEST(controlFlowContinueTargetsTest);
		TESTER_ADD_TEST(simpleBools);
		TESTER_ADD_TEST(blockDebugNamesTest);
		TESTER_ADD_TEST(simpleFunctionCalls);
		TESTER_ADD_TEST(numericLiteralsTest);
		TESTER_ADD_TEST(functionParametersTest);
		TESTER_ADD_TEST(functionEndTest);
		TESTER_ADD_TEST(voidCallTest);
		TESTER_ADD_TEST(metaFunctionsTest);
		TESTER_ADD_TEST(referencesTest);
		TESTER_ADD_TEST(tupleTest);
		TESTER_ADD_TEST(tupleTypeCoercionTest);
		TESTER_ADD_TEST(moveStateMapTest);
		TESTER_ADD_TEST(conditionalTemporaryMoveStateTest);
		TESTER_ADD_TEST(sliceTest);
		TESTER_ADD_TEST(lazyBooleanShortCircuitTest);
		TESTER_ADD_TEST(lazyBooleanChainsTest);
	}

protected:
	void beforeAll() override { dia::configureImmediatePrint(&std::cerr); }

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	/**
	 * Named actions retain their enclosing targets through nested statements. A kind-selected
	 * break can cross a named if to exit the nearest while.
	 */
	void controlFlowTargetsTest() {
		auto module = compiler::frontend::createModuleTreeFromContents(
			R"(
				fun nested(c: bool) -> i64 = {
					while outer(c) {
						while inner(c) {
							if (c) break outer;
						}
					}
					return 1;
				}
				fun nestedElse(c: bool) -> i64 = {
					while outer(c) {
						while inner(c) {
							if (not c) {
								break outer;
							} else {
								continue outer;
							}
						}
					}
					return 1;
				}
				fun namedIf(c: bool) -> i64 = {
					if region(c) { break region; }
					return 2;
				}
				fun namedBlock() -> i64 = {
					block region { break region; }
					return 3;
				}
				fun kindInIf(c: bool) -> i64 = {
					while target(c) {
						if region(c) { break while; }
					}
					return 4;
				}
				fun unnamedInBlock(c: bool) -> i64 = {
					while (c) {
						block { if (c) break; }
					}
					return 5;
				}
			)",
			"test_package"
		);

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			ASSERT_EQUAL(6u, unit.functions.size());

			auto getJumpTarget
				= [this](const compiler::mir::Function& function, std::string_view block_name) {
					  base::Optional<BlockID> result;
					  for (auto id: function.block_order) {
						  const auto& block = function.blocks[id];
						  if (not block.debug_name.has_value()
					          || block.debug_name.value().strView() != block_name)
							  continue;
						  ASSERT_NO_VALUE(result);
						  ASSERT_EQUAL(compiler::mir::Operation::Jump, block.terminator.operation);
						  result = block.terminator.arguments.at(0).get<BlockID>();
					  }
					  ASSERT_HAS_VALUE(result);
					  return result.value();
				  };

			for (usize i = 0; i < unit.functions.size(); i++) {
				auto function     = compiler::mir::lowerToPreMIRFunction(ctx, unit.functions.at(i));
				auto break_target = getJumpTarget(function, "break");
				ASSERT_HAS_VALUE(function.blocks[break_target].debug_name);
				ASSERT_EQUAL(
					base::StrID("return"), function.blocks[break_target].debug_name.value()
				);

				if (i != 1) continue;
				auto        continue_target = getJumpTarget(function, "continue");
				const auto& latch           = function.blocks[continue_target];
				ASSERT_HAS_VALUE(latch.debug_name);
				ASSERT_EQUAL(base::StrID("while.body.end"), latch.debug_name.value());
				auto        condition_id = latch.terminator.arguments.at(0).get<BlockID>();
				const auto& condition    = function.blocks[condition_id];
				ASSERT_HAS_VALUE(condition.debug_name);
				ASSERT_EQUAL(base::StrID("while.cond"), condition.debug_name.value());
				auto exit_id = condition.terminator.arguments.at(2).get<BlockID>();
				ASSERT_HAS_VALUE(function.blocks[exit_id].debug_name);
				ASSERT_EQUAL(base::StrID("return"), function.blocks[exit_id].debug_name.value());
			}
		});
	}

	void controlFlowContinueTargetsTest() {
		auto module = compiler::frontend::createModuleTreeFromContents(
			R"(
				fun implicit(c: bool) -> i64 = {
					while (c) {
						block { if (c) continue; }
					}
					return 1;
				}
				fun ifTargets(c: bool) -> i64 = {
					if region(c) { continue if; }
					else { continue region; }
					return 2;
				}
				fun breakFromElse(c: bool) -> i64 = {
					if (c) {} else { break if; }
					return 3;
				}
				fun blockTargets(c: bool) -> i64 = {
					block region {
						if (c) continue block;
						continue region;
					}
					return 4;
				}
				fun constIfTarget() -> i64 = {
					if const (true) { continue if; }
					return 5;
				}
			)",
			"test_package"
		);

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			ASSERT_EQUAL(5u, unit.functions.size());

			auto jumpTargets
				= [this](const compiler::mir::Function& function, std::string_view block_name) {
					  std::vector<BlockID> result;
					  for (auto id: function.block_order) {
						  const auto& block = function.blocks[id];
						  if (!block.debug_name.has_value()
					          || block.debug_name.value().strView() != block_name)
							  continue;
						  ASSERT_EQUAL(compiler::mir::Operation::Jump, block.terminator.operation);
						  result.push_back(block.terminator.arguments.at(0).get<BlockID>());
					  }
					  return result;
				  };

			for (usize i = 0; i < unit.functions.size(); i++) {
				auto function = compiler::mir::lowerToPreMIRFunction(ctx, unit.functions.at(i));
				auto targets  = jumpTargets(function, i == 2 ? "break" : "continue");
				ASSERT_EQUAL(i == 1 || i == 3 ? 2u : 1u, targets.size());
				base::StrID expected_name = [&] {
					switch (i) {
					case 0:
						return base::StrID("while.body.end");
					case 1:
						return base::StrID("if.cond");
					case 2:
						return base::StrID("return");
					case 3:
						return base::StrID("block.entry");
					default:
						return base::StrID("const.if.entry");
					}
				}();
				for (auto target: targets) {
					ASSERT_HAS_VALUE(function.blocks[target].debug_name);
					ASSERT_EQUAL(expected_name, function.blocks[target].debug_name.value());
				}
				if (targets.size() == 2) ASSERT_EQUAL(targets[0], targets[1]);

				auto& lowered
					= ctx.query<compiler::mir::LowerToMIRFunction>({ unit.functions.at(i) })
				          ->valueOrThrow();
				ASSERT_TRUE(lowered.validateBlockIDs().isOk());
			}
		});
	}

	/**
	 * @brief Unit test for the global in-move-state map produced by `calculateGlobalInMoveStateMap`.
	 *
	 * Uses `moveParamThenBlock(a, c)`, which moves the parameter `a` in the entry block and then
	 * branches. The map is computed on the pre-lifetime MIR (so the move flag is present but no
	 * use-after-move validation runs), and we assert:
	 *  - `a` is alive on entry (it is a parameter),
	 *  - some successor block sees `a` as `Moved` with exactly one reaching move site.
	 */
	void moveStateMapTest() {
		auto [module, scope] = getModule(fs::File(path("modules/move_lifetime")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			base::Optional<CRef<compiler::helios::HOUTFunction>> target;
			for (const auto& fn: unit.functions)
				if (fn->declaration->original_name.strView() == "moveParamThenBlock") target = fn;
			ASSERT_HAS_VALUE(target);

			auto pre_mir = compiler::mir::lowerToPreMIRFunction(ctx, target.value());

			// Find the first parameter local (`a`).
			base::Optional<compiler::mir::LocalID> a_id;
			for (const auto& local: pre_mir.local_list)
				if (local.parameter_index.has_value() && local.parameter_index.value() == 0)
					a_id = local.id;
			ASSERT_HAS_VALUE(a_id);

			// Build predecessor lists, same as constructLifetimePassArgs does.
			base::HashMap<compiler::mir::BlockID, std::vector<compiler::mir::BlockID>> preds;
			for (auto block_id: pre_mir.block_order)
				for (auto succ:
				     compiler::mir::getTerminatorSuccessors(pre_mir.blocks.at(block_id)->terminator))
					preds.put(succ).first->second.push_back(block_id);

			auto locals_by_scope = compiler::mir::collectLocalsByScope(pre_mir);
			auto move_states     = compiler::mir::MoveStateData::calculateGlobalInMoveStateMap(
                pre_mir, preds, locals_by_scope
            );

			// `a` is a parameter, so it is alive at the entry block.
			auto entry     = pre_mir.block_order.front();
			auto entry_map = move_states.block_in_move_state.atMaybe(entry);
			ASSERT_HAS_VALUE(entry_map);
			auto a_at_entry = entry_map.value()->stateOf(a_id.value());
			ASSERT_HAS_VALUE(a_at_entry);
			ASSERT_TRUE(a_at_entry.value()->status == compiler::mir::MoveStatus::Alive);

			// After the unconditional move in the entry block, at least one successor block must
			// observe `a` as `Moved` with exactly one reaching move site.
			bool found_moved = false;
			for (const auto& [block_id, map]: move_states.block_in_move_state) {
				auto state = map.stateOf(a_id.value());
				if (state.has_value() && state.value()->status == compiler::mir::MoveStatus::Moved
				    && state.value()->move_sites.size() == 1)
					found_moved = true;
			}
			ASSERT_TRUE(found_moved);
		});
	}

	/**
	 * @brief A temporary built on only one control-flow path is `MaybeMoved` where the paths merge.
	 *
	 * `condTemporary(c)` lowers `c or makeR(1).a == 0` lazily, so the `R` temporary of the
	 * right-hand side is constructed on one path and never touched on the other. It lives in the
	 * scope of the whole expression, so the join has to report it as `MaybeMoved` (which is what
	 * gives it a conditional destructor) instead of treating it as uninitialized.
	 */
	void conditionalTemporaryMoveStateTest() {
		auto [module, scope] = getModule(fs::File(path("modules/move_lifetime")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			base::Optional<CRef<compiler::helios::HOUTFunction>> target;
			for (const auto& fn: unit.functions)
				if (fn->declaration->original_name.strView() == "condTemporary") target = fn;
			ASSERT_HAS_VALUE(target);

			auto pre_mir = compiler::mir::lowerToPreMIRFunction(ctx, target.value());

			base::HashMap<compiler::mir::BlockID, std::vector<compiler::mir::BlockID>> preds;
			for (auto block_id: pre_mir.block_order)
				for (auto succ:
				     compiler::mir::getTerminatorSuccessors(pre_mir.blocks.at(block_id)->terminator))
					preds.put(succ).first->second.push_back(block_id);

			auto locals_by_scope = compiler::mir::collectLocalsByScope(pre_mir);
			auto move_states     = compiler::mir::MoveStateData::calculateGlobalInMoveStateMap(
                pre_mir, preds, locals_by_scope
            );

			// The `R` temporary holding the result of `makeR(1)`.
			base::Optional<compiler::mir::LocalID> tmp_id;
			for (const auto& local: pre_mir.local_list)
				if (local.type.getType().getKind() == compiler::tsh::Kind::Class) {
					ASSERT_TRUE(tmp_id.empty());
					tmp_id = local.id;
				}
			ASSERT_HAS_VALUE(tmp_id);

			// Both paths reach the merge block, and there the temporary is only maybe-initialized.
			bool found_maybe_moved = false;
			for (const auto& [block_id, map]: move_states.block_in_move_state) {
				auto state = map.stateOf(tmp_id.value());
				if (state.has_value()
				    && state.value()->status == compiler::mir::MoveStatus::MaybeMoved)
					found_maybe_moved = true;
			}
			ASSERT_TRUE(found_maybe_moved);
		});
	}

	void simpleTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_simple_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			auto& functions = unit.functions;
			ASSERT_EQUAL(4, functions.size());
			ASSERT_EQUAL(base::StrID("foo1"), functions.at(0)->declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo2"), functions.at(1)->declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo3"), functions.at(2)->declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo4"), functions.at(3)->declaration->original_name);

			auto foo1_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(0));
			auto foo2_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(1));
			auto foo3_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(2));
			auto foo4_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(3));

			ASSERT_EQUAL(foo1_mir.name, base::StrID("foo1"));
			ASSERT_EQUAL(foo2_mir.name, base::StrID("foo2"));
			ASSERT_EQUAL(foo3_mir.name, base::StrID("foo3"));
			ASSERT_EQUAL(foo4_mir.name, base::StrID("foo4"));

			ASSERT_EQUAL(foo1_mir.block_order.size(), 1);
			ASSERT_EQUAL(foo2_mir.block_order.size(), 2);
			ASSERT_EQUAL(foo3_mir.block_order.size(), 6);
			ASSERT_EQUAL(foo4_mir.block_order.size(), 2);

			// This doesn't test much other then that the code doesn't crash/throw exceptions.
			// It also make debug_prints covered by tests.
			std::stringstream all_functions;
			foo1_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo1_mir.validateBlockIDs().isOk());
			foo2_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo2_mir.validateBlockIDs().isOk());
			foo3_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo3_mir.validateBlockIDs().isOk());
			foo4_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo4_mir.validateBlockIDs().isOk());
		});
	}

	void simpleVarTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0)->declaration->original_name);

			auto globals = unit.glob_data;
			ASSERT_EQUAL(2, globals.size());
			ASSERT_EQUAL(base::StrID("c"), globals.at(0)->original_name);

			auto& c_data
				= ctx.query<compiler::mir::LowerGlobalData>({ globals.at(0) })->valueOrThrow();
			auto c_ctor_dtor = std::get<compiler::mir::MIRCtorDtorPair>(c_data.initial_value);
			ASSERT_TRUE(c_ctor_dtor.constructor->name.strView() == "constructor_of_c");
			// `c` is an i64, it is trivially destructible, so it gets no destructor.
			ASSERT_TRUE(c_ctor_dtor.destructor.empty());

			auto foo_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(0));
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			// Test locals:
			ASSERT_EQUAL(foo_mir.local_list.size(), 3);

			auto i64_type = getIntegralType(ctx, 64, Signed);

			{
				auto a = foo_mir.local_list[0];
				ASSERT_EQUAL(a->getName(), "a");
				ASSERT_EQUAL(a->type.getType(), i64_type);
			}
			{
				auto b = foo_mir.local_list[1];
				ASSERT_EQUAL(b->getName(), "b");
				ASSERT_EQUAL(b->type.getType(), i64_type);
			}
			{
				auto b = foo_mir.local_list[2];
				ASSERT_EQUAL(b->getName(), "b");
				ASSERT_EQUAL(b->type.getType(), i64_type);
			}
			// Test code generation:

			ASSERT_EQUAL(foo_mir.block_order.size(), 8);

			// @note: instruction count does not include terminator instruction:

			using enum compiler::mir::Operation;

			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].id, foo_mir.block_order[0]);

			// those assertions might change when we improve mir generation:
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.size(), 3);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.at(0).operation, Assign);
			// Check that the first instruction assigns to a global
			{
				const auto& instr = foo_mir.blocks[BlockID(7)].instructions.at(0);
				ASSERT_TRUE(instr.output.value().isGlobal());
			}
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.at(1).operation, Cast);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].terminator.operation, Jump);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].instructions.size(), 2);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].instructions.at(0).operation, Cast);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].terminator.operation, Jump);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(5)].terminator.operation, Branch);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(2)].terminator.operation, Branch);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(4)].instructions.size(), 2);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(4)].instructions.at(0).operation, Cast);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(4)].terminator.operation, Jump);

			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			// Simple assignment tests
			auto goo_mir = compiler::mir::lowerToPreMIRFunction(ctx, functions.at(1));
			ASSERT_EQUAL(goo_mir.name, base::StrID("goo"));

			ASSERT_EQUAL(goo_mir.local_list.size(), 1);

			// assert that in the first block we have the two assignments and the call
			ASSERT_EQUAL(goo_mir.block_order.size(), 1);

			auto first_block_id = goo_mir.block_order[0];

			ASSERT_EQUAL(goo_mir.blocks[first_block_id].instructions.size(), 4);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(0).operation,
				compiler::mir::Operation::Assign
			);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(1).operation,
				compiler::mir::Operation::Assign
			);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(2).operation,
				compiler::mir::Operation::Call
			);
		});
	}

	void testTerminatorSuccessors() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL_PRINT(3, functions.size());
			ASSERT_EQUAL_PRINT(base::StrID("foo"), functions.at(0)->declaration->original_name);

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->valueOrThrow();

			ASSERT_EQUAL_PRINT(foo_mir.name, base::StrID("foo"));
			ASSERT_EQUAL_PRINT(foo_mir.block_order.size(), 11);
			ASSERT_EQUAL_PRINT(foo_mir.local_list.size(), 3);

			auto get_block_terminator
				= [&](u64 block_id) { return foo_mir.blocks[BlockID(block_id)].terminator; };
			auto get_block_successors = [&](u64 block_id) {
				return getTerminatorSuccessors(get_block_terminator(block_id));
			};

			using BlockList = std::vector<BlockID>;
			ASSERT_EQUAL(get_block_successors(1), BlockList{});
			ASSERT_EQUAL(get_block_successors(2), BlockList{ BlockID{ 4 } COMMA BlockID{ 3 } });

			ASSERT_EQUAL(get_block_successors(3), BlockList{ BlockID{ 1 } });

			// Here, the order does not matter.
			// If it breaks because the order changes,
			// the check has to be changed to an order-free assertion.
			//
			// Both branches enter a scope of their own, so the destructor pass splits every one
			// of their edges into an intermediate block that jumps on to the original target.
			ASSERT_EQUAL(get_block_successors(4), BlockList{ BlockID{ 11 } COMMA BlockID{ 12 } });
			ASSERT_EQUAL(get_block_successors(11), BlockList{ BlockID{ 3 } });
			ASSERT_EQUAL(get_block_successors(12), BlockList{ BlockID{ 2 } });

			ASSERT_EQUAL(get_block_successors(5), BlockList{ BlockID{ 9 } COMMA BlockID{ 10 } });
			ASSERT_EQUAL(get_block_successors(9), BlockList{ BlockID{ 6 } });
			ASSERT_EQUAL(get_block_successors(10), BlockList{ BlockID{ 4 } });

			ASSERT_EQUAL(get_block_successors(6), BlockList{ BlockID{ 5 } });
			ASSERT_EQUAL(get_block_successors(7), BlockList{ BlockID{ 5 } });
		});
	}

	void simpleBools() {
		auto [module, scope] = getModule(fs::File(path("modules/booleans")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(2, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->valueOrThrow();
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			bool  saw_true     = false;
			bool  saw_false    = false;
			usize branch_count = 0;
			for (auto id: foo_mir.block_order) {
				const auto& terminator = foo_mir.blocks[id].terminator;
				if (terminator.operation != compiler::mir::Operation::Branch) continue;
				auto condition
					= terminator.arguments.at(0).get<compiler::mir::MIRConstant>().value.get<bool>();
				if (condition.value())
					saw_true = true;
				else
					saw_false = true;
				branch_count++;
			}
			ASSERT_EQUAL(2u, branch_count);
			ASSERT_TRUE(saw_true);
			ASSERT_TRUE(saw_false);

			// Don't go into details of the second function. Just validate block IDs.
			auto& goo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(1) })->valueOrThrow();
			ASSERT_TRUE(goo_mir.validateBlockIDs().isOk());
		});
	}

	/**
	 * @brief Blocks created by the lowering carry a debug name saying what they were generated
	 * for, and `Function::debugPrint` shows that name right after the block id.
	 */
	void blockDebugNamesTest() {
		auto [module, scope] = getModule(fs::File(path("modules/booleans")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(2, functions.size());

			auto collect_names = [](const compiler::mir::Function& function) {
				std::set<std::string> names;
				for (const auto block_id: function.block_order) {
					const auto& block = function.blocks[block_id];
					if (block.debug_name.has_value()) names.emplace(block.debug_name.value().str());
				}
				return names;
			};

			auto assert_has_name = [this](
									   const std::set<std::string>& names,
									   const std::string_view       expected,
									   const std::string_view       function_name
								   ) {
				assertTrue(
					names.contains(std::string{ expected }),
					base::strConcat(
						"Expected a block named `", expected, "` in `", function_name, "`"
					)
				);
			};

			// `foo` is two `if`s in a row, so it only has condition/then/else blocks plus the one
			// holding `FunctionEnd`.
			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->valueOrThrow();
			const auto foo_names = collect_names(foo_mir);
			for (const std::string_view expected:
			     { "function_end", "if.cond", "if.then", "if.else" })
				assert_has_name(foo_names, expected, "foo");

			// Nothing but the lowering creates blocks here, so every block of `foo` is named.
			for (const auto block_id: foo_mir.block_order)
				ASSERT_HAS_VALUE(foo_mir.blocks[block_id].debug_name);

			// `goo` ends with an `if ... then ... else` expression, which lowers as a ternary.
			auto& goo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(1) })->valueOrThrow();
			const auto                  goo_names = collect_names(goo_mir);
			static constexpr std::array TERNARY_NAMES
				= { "ternary.cond", "ternary.then", "ternary.else" };
			for (const std::string_view expected: TERNARY_NAMES)
				assert_has_name(goo_names, expected, "goo");

			// The printed MIR shows the name in parentheses after the block id.
			std::stringstream printed;
			foo_mir.debugPrint(printed);
			const auto printed_str = printed.str();
			assertTrue(
				printed_str.find("(if.then)") != std::string::npos,
				base::strConcat("Block name missing from the printed MIR:\n", printed_str)
			);
		});
	}

	void simpleFunctionCalls() {
		auto [module, scope] = getModule(fs::File(path("modules/function_calls")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(4, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(2) })->valueOrThrow();
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			u64 count_of_calls = 0;

			static std::array functions_to_call = {
				base::StrID("arg1"), base::StrID("arg0"), base::StrID("arg1"),
				base::StrID("arg0"), base::StrID("arg1"), base::StrID("arg1"),
			};

			auto entry_block = foo_mir.block_order[0];
			for (auto& instruction: foo_mir.blocks[entry_block].instructions) {
				if (instruction.operation == compiler::mir::Operation::Call) {
					auto callee
						= instruction.arguments.at(0).get<compiler::mir::MIRFunctionLiteral>();
					ASSERT_EQUAL(
						compiler::helios::name(callee.helios_id),
						functions_to_call.at(count_of_calls)
					);
					count_of_calls++;
				}
			}

			ASSERT_EQUAL(count_of_calls, 6);
		});
	}

	void numericLiteralsTest() {
		auto [module, scope] = getModule(fs::File(path("modules/numeric_literals")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& main_fun = unit.functions.at(0);
			ASSERT_EQUAL(main_fun->declaration->original_name, base::StrID("main"));
			auto& main_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ main_fun })->valueOrThrow();

			auto        entry_block_id = main_mir.block_order.front();
			const auto& entry_block    = main_mir.blocks[entry_block_id];

			bool found_a = false, found_b = false, found_c = false, found_d = false;

			for (const auto& instr: entry_block.instructions) {
				if (instr.operation != compiler::mir::Operation::Assign) continue;
				const auto& output_place = instr.output.value();
				const auto& local_ref    = output_place.getBase<compiler::mir::MIRLocalRef>();
				auto        var_name     = local_ref->getName();

				const auto& value_arg = instr.arguments.at(0);
				const auto& constant  = value_arg.get<compiler::mir::MIRConstant>();
				const auto& numeric_val
					= constant.value.get<compiler::numeric_value::NumericValue>();

				if (var_name == "a") {
					ASSERT_EQUAL(numeric_val->get<i16>(), 123);
					found_a = true;
				} else if (var_name == "b") {
					ASSERT_EQUAL(numeric_val->get<u32>(), 4'000'000'000);
					found_b = true;
				} else if (var_name == "c") {
					ASSERT_EQUAL(numeric_val->get<f32>(), 1.25f);
					found_c = true;
				} else if (var_name == "d") {
					ASSERT_EQUAL(numeric_val->get<f64>(), 987.654);
					found_d = true;
				}
			}

			assertTrue(found_a, "Assignment to 'a' was not found in MIR");
			assertTrue(found_b, "Assignment to 'b' was not found in MIR");
			assertTrue(found_c, "Assignment to 'c' was not found in MIR");
			assertTrue(found_d, "Assignment to 'd' was not found in MIR");
		});
	}

	void functionParametersTest() {
		auto [module, scope] = getModule(fs::File(path("modules/function_with_parameters")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(1, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->valueOrThrow();
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			auto i16_type = getIntegralType(ctx, 16, Signed);
			auto i32_type = getIntegralType(ctx, 32, Signed);
			auto i64_type = getIntegralType(ctx, 64, Signed);

			const auto& locals = foo_mir.local_list;

			bool was_x = false;
			bool was_y = false;
			bool was_z = false;

			for (auto& local: locals) {
				if (local.getName() == "x") {
					ASSERT_TRUE(not was_x);
					ASSERT_EQUAL(local.parameter_index.value(), 0);
					ASSERT_EQUAL(local.type.getType(), i16_type);
					was_x = true;
				} else if (local.getName() == "y") {
					ASSERT_TRUE(not was_y);
					ASSERT_EQUAL(local.parameter_index.value(), 1);
					ASSERT_EQUAL(local.type.getType(), i32_type);
					was_y = true;
				} else if (local.getName() == "z") {
					ASSERT_TRUE(not was_z);
					ASSERT_EQUAL(local.parameter_index.value(), 2);
					ASSERT_EQUAL(local.type.getType(), i64_type);
					was_z = true;
				} else {
					ASSERT_TRUE(local.parameter_index.empty());
				}
			}

			ASSERT_TRUE(was_x and was_y and was_z);

			// check if value used in the function body is indeed the parameter we expect:
			for (auto block_id: foo_mir.block_order) {
				const auto& block          = foo_mir.blocks[block_id];
				auto        validate_value = [&](const compiler::mir::MIRValue& value) {
                    if (value.isLocal()) {
                        if (const auto local = value.get<compiler::mir::MIRPlace>()
                                                   .getBase<compiler::mir::MIRLocalRef>();
                            local->parameter_index.has_value())
                            ASSERT_EQUAL(local->parameter_index.value(), 2);
                    }
				};

				for (const auto& instr: block.instructions) {
					// Destructors (conditional or unconditional) legitimately reference any local,
					// including parameters other than the one used in the body.
					if (instr.operation == compiler::mir::Operation::DestructIf
					    || instr.operation == compiler::mir::Operation::Destruct)
						continue;
					for (const auto& arg: instr.arguments) validate_value(arg);
				}
				for (const auto& arg: block.terminator.arguments) validate_value(arg);
			}
		});
	}

	void functionEndTest() {
		auto [module, scope] = getModule(fs::File(path("modules/function_end_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL_PRINT(4, functions.size());
			ASSERT_EQUAL_PRINT(
				functions.at(0)->declaration->original_name, base::StrID("missing_return")
			);
			ASSERT_EQUAL_PRINT(
				functions.at(1)->declaration->original_name, base::StrID("should_add_retvoid")
			);
			ASSERT_EQUAL_PRINT(
				functions.at(2)->declaration->original_name, base::StrID("unreachable_end")
			);
			ASSERT_EQUAL_PRINT(functions.at(3)->declaration->original_name, base::StrID("empty"));

			ASSERT_TRUE(
				ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->hasFailed()
			);

			auto& should_add_retvoid_fun
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(1) })->valueOrThrow();
			ASSERT_TRUE(should_add_retvoid_fun.validateBlockIDs().isOk());
			std::stringstream foo_str;
			should_add_retvoid_fun.debugPrint(foo_str);

			auto& last_block
				= should_add_retvoid_fun.blocks[should_add_retvoid_fun.block_order.back()];
			ASSERT_EQUAL_PRINT(
				last_block.terminator.operation, compiler::mir::Operation::ReturnVoid
			);

			auto& unreachable_end_fun
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(2) })->valueOrThrow();
			ASSERT_TRUE(unreachable_end_fun.validateBlockIDs().isOk());
			unreachable_end_fun.debugPrint(foo_str);
			// A `while` and an `if`, each of whose four edges enters a scope of its own and so
			// gets an intermediate block from the destructor pass.
			ASSERT_EQUAL_PRINT(unreachable_end_fun.block_order.size(), 11);

			auto& empty
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(3) })->valueOrThrow();
			ASSERT_EQUAL(empty.block_order.size(), 1);
		});
	}

	/**
	 * @brief A call to a `-> void` function never returns, so its block ends with `Unreachable`
	 * and everything the source wrote after the call (here `return 42`) is unreachable and gets
	 * eliminated.
	 */
	void voidCallTest() {
		auto [module, _] = getModule(fs::File(path("modules/function_calls")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			base::Optional<CRef<compiler::helios::HOUTFunction>> target;
			for (const auto& fn: unit.functions)
				if (fn->declaration->original_name.strView() == "callDiverges") target = fn;
			ASSERT_HAS_VALUE(target);

			auto& function
				= ctx.query<compiler::mir::LowerToMIRFunction>({ target.value() })->valueOrThrow();

			auto& last_block = function.blocks[function.block_order.back()];
			ASSERT_EQUAL_PRINT(
				last_block.terminator.operation, compiler::mir::Operation::Unreachable
			);

			for (auto block_id: function.block_order)
				ASSERT_TRUE(
					function.blocks[block_id].terminator.operation
					!= compiler::mir::Operation::ReturnValue
				);
		});
	}

	void metaFunctionsTest() {
		auto [module, scope] = getModule(fs::File(path("modules/meta_functions")));

		auto size_of_sym  = getChain("sizeOf", scope).back();
		auto align_of_sym = getChain("alignOf", scope).back();

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;

			compiler::tsh::SymbolType<> meta_type{
				getMetaType(),
				compiler::tsh::ReferenceKind::Direct,
				compiler::tsh::Mutability::Mutable,
			};

			using enum compiler::mir::Operation;
			using MK = compiler::mir::MetaKind;

			// Meta operations are a single `Meta` op parametrized by a `MetaKind` in `extra_params`.
			auto meta_kind = [](const auto& instr) {
				return std::get<compiler::mir::MetaParameters>(instr.extra_params).kind;
			};

			auto check_meta_function
				= [&](CRef<compiler::helios::HOUTFunction> fun, MK kind, usize arg_size) {
					  auto& mir_fun
						  = ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->valueOrThrow();
					  const auto& block = mir_fun.blocks[mir_fun.block_order[0]];
					  const auto& instr = block.instructions[0];
					  ASSERT_TRUE(instr.operation == MetaTypeOperation);
					  ASSERT_TRUE(meta_kind(instr) == kind);
					  ASSERT_EQUAL(instr.arguments.size(), arg_size);
					  ASSERT_TRUE(instr.arguments[0].isLocal());
				  };

			{
				CRef fun
					= &ctx.query<compiler::helios::QueryCodeOfFun>(size_of_sym)->valueOrThrow();
				check_meta_function(fun, MK::SizeOf, 1);
			}
			{
				CRef fun
					= &ctx.query<compiler::helios::QueryCodeOfFun>(align_of_sym)->valueOrThrow();
				check_meta_function(fun, MK::AlignOf, 1);
			}

			for (CRef<compiler::helios::HOUTFunction> fun: functions) {
				if (fun->declaration->original_name.str() == "createBox") {
					check_meta_function(fun, MK::CreateBox, 1);
				} else if (fun->declaration->original_name.str() == "createRef") {
					check_meta_function(fun, MK::CreateRef, 1);
				} else if (fun->declaration->original_name.str() == "createPtr") {
					check_meta_function(fun, MK::CreatePtr, 1);
				} else if (fun->declaration->original_name.str() == "createCPtr") {
					check_meta_function(fun, MK::CreateCPtr, 1);
				} else if (fun->declaration->original_name.str() == "createManyPtr") {
					check_meta_function(fun, MK::CreateManyPtr, 1);
				} else if (fun->declaration->original_name.str() == "createSlice") {
					check_meta_function(fun, MK::CreateSlice, 1);
				} else if (fun->declaration->original_name.str() == "createConst") {
					check_meta_function(fun, MK::CreateConst, 1);
				} else if (fun->declaration->original_name.str() == "createVariant") {
					check_meta_function(fun, MK::CreateVariant, 4);
				} else if (fun->declaration->original_name.str() == "createTuple") {
					check_meta_function(fun, MK::CreateTuple, 4);
				} else if (fun->declaration->original_name.str() == "megaType") {
					auto& mir_fun
						= ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->valueOrThrow();

					for (const auto& local: mir_fun.local_list) ASSERT_EQUAL(local.type, meta_type);
					const auto& block = mir_fun.blocks[mir_fun.block_order[0]];

					int  create_variant_count     = 0;
					int  create_tuple_count       = 0;
					bool create_tuple_5_arg_found = false;
					bool call_found               = false;
					for (const auto& instr: block.instructions) {
						if (instr.operation == MetaTypeOperation
						    && meta_kind(instr) == MK::CreateTuple) {
							if (instr.arguments.size() == 5) {
								// When a big tuple instruction is found, it should be preceeded
								// with two inner tuple create instructions and one inner variant
								// create instruction.
								ASSERT_TRUE(create_tuple_count == 2);
								ASSERT_TRUE(create_variant_count == 1);
								create_tuple_5_arg_found = true;
							}
							create_tuple_count++;
						} else if (instr.operation == MetaTypeOperation
						           && meta_kind(instr) == MK::CreateVariant) {
							// If variant is created, two preceding tuple creating instructions
							// should exist.
							ASSERT_TRUE(create_tuple_count == 2);
							create_variant_count++;

						} else if (instr.operation == Call) {
							call_found = true;
						}
					}

					ASSERT_EQUAL(create_variant_count, 1);
					ASSERT_EQUAL(create_tuple_count, 3);
					ASSERT_TRUE(create_tuple_5_arg_found);
					ASSERT_TRUE(call_found);
				}
			}
		});
	}

	void referencesTest() {
		auto [module, scope] = getModule(fs::File(path("modules/references")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& hout_func = unit.functions.at(0);
			auto& mir_func  = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ hout_func })
			                     ->valueOrThrow();

			bool found_simple_address_of     = false;
			bool found_address_of_with_deref = false;
			bool found_complex_assignment    = false;
			using namespace compiler::mir;
			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					//  var r = &x;
					if (instr.operation == Operation::AddressOf) {
						auto& arg = instr.arguments[0].get<MIRPlace>();
						if (arg.projection_chain.empty()) {
							found_simple_address_of = true;
						}
						// var r = &p.x;
						else if (arg.projection_chain.size() == 2) {
							bool has_deref = v_matches(
								arg.projection_chain[0].storage, MIRPlace::DerefProjection
							);
							bool has_field = v_matches(
								arg.projection_chain[1].storage, MIRPlace::FieldProjection
							);
							if (has_deref && has_field) {
								auto field = std::get<MIRPlace::FieldProjection>(
									arg.projection_chain[1].storage
								);
								if (compiler::helios::name(field.field_id) == base::StrID("x"))
									found_address_of_with_deref = true;
							}
						}
					}
					// ref_wrapper -> Deref -> Field(p) -> Deref -> Field(x) -> Deref
					if (instr.operation == Operation::Assign && instr.output.has_value()) {
						auto& out_place = instr.output.value();

						if (out_place.projection_chain.size() == 5) {
							const auto& chain = out_place.projection_chain;
							bool        pattern_ok
								= v_matches(chain[0].storage, MIRPlace::DerefProjection)
							   && v_matches(chain[1].storage, MIRPlace::FieldProjection)
							   && v_matches(chain[2].storage, MIRPlace::DerefProjection)
							   && v_matches(chain[3].storage, MIRPlace::FieldProjection)
							   && v_matches(chain[4].storage, MIRPlace::DerefProjection);

							if (pattern_ok) {
								auto f_p = std::get<MIRPlace::FieldProjection>(chain[1].storage);
								auto f_x = std::get<MIRPlace::FieldProjection>(chain[3].storage);

								if (compiler::helios::name(f_p.field_id) == base::StrID("p")
								    && compiler::helios::name(f_x.field_id) == base::StrID("x")) {
									auto constant = instr.arguments[0].get<MIRConstant>();
									auto num
										= constant.value.get<compiler::numeric_value::NumericValue>(
										);
									if (num->get<i32>() == 999) found_complex_assignment = true;
								}
							}
						}
					}
				}
			}

			ASSERT_TRUE(found_simple_address_of);
			ASSERT_TRUE(found_address_of_with_deref);
			ASSERT_TRUE(found_complex_assignment);
		});
	}

	void tupleTest() {
		auto [module, scope] = getModule(fs::File(path("modules/tuples")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& hout_func = unit.functions.at(0);

			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ hout_func })
			                     ->valueOrThrow();

			bool found_tuple_ctor_call = false;

			using namespace compiler::mir;

			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					switch (instr.operation) {
					case Operation::Call: {
						auto  ctor_id = instr.arguments.at(0).get<MIRFunctionLiteral>().helios_id;
						auto& fun_decl
							= ctx.query<compiler::helios::QueryDeclOfFun>(ctor_id)->valueOrThrow();
						if (fun_decl.return_type.getType().getKind() == compiler::tsh::Kind::Tuple)
							found_tuple_ctor_call = true;
						break;
					}
					default:
						break;
					}
				}
			}

			ASSERT_TRUE(found_tuple_ctor_call);
		});
	}

	/**
	 * @brief Lowering of tuples coerced to a tuple type with `type` components.
	 *
	 * Every element that is not a type yet has to be lifted to one. An element of unit type has no
	 * runtime representation to lift, so it lowers to the unit type itself - the lowering must
	 * still produce a well formed `MetaTypeOperation`/`CreateTuple` instruction for the whole
	 * tuple instead of falling through to a plain value lowering.
	 */
	void tupleTypeCoercionTest() {
		auto [module, scope] = getModule(fs::File(path("modules/tuple_type_coercion")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& hout_func = unit.functions.at(0);

			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ hout_func })
			                     ->valueOrThrow();

			ASSERT_TRUE(mir_func.validateBlockIDs().isOk());

			using enum compiler::mir::Operation;
			using MK = compiler::mir::MetaKind;

			auto meta_kind = [](const auto& instr) {
				return std::get<compiler::mir::MetaParameters>(instr.extra_params).kind;
			};

			// Whether the argument is the unit type, i.e. a unit-typed element lifted to a type.
			auto is_unit_type_constant = [](const compiler::mir::MIRValue& argument) {
				if (!argument.isConstant()) return false;
				auto stored = argument.get<compiler::mir::MIRConstant>()
				                  .value.get<compiler::tsh::SymbolType<>>();
				return stored.has_value()
				    && stored->getType().getKind() == compiler::tsh::Kind::Unit;
			};

			usize create_tuple_count       = 0;
			usize lifted_unit_elements     = 0;
			usize lifted_from_materialised = 0;

			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					if (instr.operation == MetaTypeOperation
					    && meta_kind(instr) == MK::CreateTuple) {
						create_tuple_count++;
						continue;
					}
					// A tuple constructor call: the function, followed by one argument per element.
					if (instr.operation != Call) continue;

					bool lifts_unit       = false;
					bool reads_from_tuple = false;
					for (usize i = 1; i < instr.arguments.size(); i++) {
						if (is_unit_type_constant(instr.arguments[i])) {
							lifts_unit = true;
							lifted_unit_elements++;
						}
						// A field read, so the source tuple had to be materialised first.
						if (instr.arguments[i].isLocal()
						    && instr.arguments[i].get<compiler::mir::MIRPlace>().hasProjections())
							reads_from_tuple = true;
					}

					if (lifts_unit && reads_from_tuple) lifted_from_materialised++;
				}
			}

			// `nested` builds its inner `(i32, i64)` type before the outer tuple.
			assertEqual(1u, create_tuple_count, "Expected one nested tuple type to be created.");

			// One in each of `literal`, `lifted` and `nested`. The unit element of `value` keeps
			// its unit type, so it is not lifted.
			assertEqual(
				3u, lifted_unit_elements, "Expected every unit element to be lifted to a type."
			);
			// `lifted` reads its unit element back out of the materialised `value` tuple.
			assertEqual(
				1u,
				lifted_from_materialised,
				"Expected the unit element of a materialised tuple to be lifted to a type."
			);
		});
	}

	/**
	 * @brief `and` / `or` lower lazily: the right-hand side sits in its own block, which only one
	 * of the two outcomes of the left-hand side leads to.
	 */
	void lazyBooleanShortCircuitTest() {
		using compiler::mir::Operation;
		namespace test_utils = compiler::mir::test_utils;

		test_utils::checkLoweredModule(
			R"(fun probe() -> bool = { return true; }
               fun lazyOr(a: bool) -> bool = { return a or probe(); }
               fun lazyAnd(a: bool) -> bool = { return a and probe(); })",
			[this](query::Context&, const compiler::mir::MIRUnit& unit) {
				for (const std::string_view name: { "lazyOr", "lazyAnd" }) {
					const auto& function = *test_utils::functionOfUnit(unit, name);

					const auto rhs_blocks
						= test_utils::blocksWithOperation(function, Operation::Call);
					const auto rhs_block = *rhs_blocks.begin();

					// The left-hand side has to decide where to go, so its block branches.
					assertEqual(
						1u,
						test_utils::countTerminators(function, Operation::Branch),
						base::strConcat("Expected a single branch in `", name, "`")
					);

					bool evaluating_path = false;
					bool skipping_path   = false;
					for (const auto& path: test_utils::pathsFromEntry(function))
						if (std::ranges::find(path, rhs_block) != path.end())
							evaluating_path = true;
						else
							skipping_path = true;

					assertTrue(
						evaluating_path,
						base::strConcat("No path of `", name, "` evaluates the right-hand side")
					);
					assertTrue(
						skipping_path,
						base::strConcat("Every path of `", name, "` evaluates the right-hand side")
					);
				}
			}
		);
	}

	/**
	 * @brief Chained `and` / `or` lower to a chain of branches, and the temporary holding the
	 * result is initialized on every path.
	 */
	void lazyBooleanChainsTest() {
		using compiler::mir::Operation;
		namespace test_utils = compiler::mir::test_utils;

		test_utils::checkLoweredModule(
			R"(fun probe() -> bool = { return true; }
               fun orChain(a: bool) -> bool = { return a or probe() or probe(); }
               fun andChain(a: bool) -> bool = { return a and probe() and probe(); }
               fun mixedChain(a: bool) -> bool = { return a and probe() or probe(); })",
			[this](query::Context&, const compiler::mir::MIRUnit& unit) {
				for (const std::string_view name: { "orChain", "andChain", "mixedChain" }) {
					const auto& function = *test_utils::functionOfUnit(unit, name);

					ASSERT_TRUE(function.validateBlockIDs().isOk());

					// Three operands, so the first two each branch on their own value.
					assertEqual(
						2u,
						test_utils::countTerminators(function, Operation::Branch),
						base::strConcat("Expected two branches in `", name, "`")
					);

					// Every block of a lowered function is reachable; an operand block that lost
				    // its incoming edge would show up here.
					const auto reachable = test_utils::reachableBlocks(function);
					for (const auto block_id: function.block_order)
						assertTrue(
							reachable.contains(block_id),
							base::strConcat(
								"Block ", u64(block_id), " of `", name, "` is not reachable"
							)
						);
				}
			}
		);
	}

	void sliceTest() {
		// Test that without STD library, slice type access will not work.
		auto [module, scope] = getModule(fs::File(path("modules/slices")));

		withContextDo([&](query::Context& ctx) {
			auto hout_unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto mir_unit  = compiler::mir::lowerToMIRUnit(ctx, &hout_unit->valueOrPanic());
			ASSERT_TRUE(mir_unit.hasFailed());
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
