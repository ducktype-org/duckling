#include "stmt_lowering.hpp"

#include "expr_lowering.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/collections/optional.hpp>

#include <ranges>
#include <utility>
#include <vector>

namespace compiler::mir {
	namespace {
		struct ControlFlowTarget final {
			helios::code::ControlFlowKind kind;
			base::Optional<helios::SymID> id;

			BlockBuilderRef break_target;
			BlockBuilderRef continue_target;
		};

		using ControlFlowTargets = std::vector<ControlFlowTarget>;

		StmtLowerRes lowerStmtWithControlFlowTargets(
			const hc::Stmt&    stmt,
			BlockBuilderRef    continuation,
			FunctionBuilder&   function,
			ScopeRef           parent_scope,
			ControlFlowTargets targets
		);

		StmtLowerRes lowerCodeBlockWithControlFlowTargets(
			const hc::CodeBlock& code_block,
			BlockBuilderRef      continuation,
			FunctionBuilder&     function,
			ScopeRef             parent_scope,
			ControlFlowTargets   targets
		);
	}

	/**
	 * @brief Visitor that implements actual logic of lowering statements.
	 * @note The result of the visitor is stored in out member.
	 */
	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		BlockBuilderRef continuation;

		FunctionBuilder& function;

		/**
		 * Scope of the parent.
		 */
		ScopeRef parent_scope;

		ControlFlowTargets targets;

		StmtBlockVisitor(
			BlockBuilderRef    continuation,
			FunctionBuilder&   function,
			ScopeRef           parent_scope,
			ControlFlowTargets targets
		):
			  continuation(continuation),
			  function(function),
			  parent_scope(parent_scope),
			  targets(std::move(targets)) {}

		base::Optional<StmtLowerRes> out;

		void output(const StmtLowerRes& value) {
			CORE_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		[[nodiscard]] const ControlFlowTarget* findTarget(
			const hc::ControlFlowTargetSelector& selector
		) const {
			const auto* named = std::get_if<hc::NamedTarget>(&selector);
			const auto* by_kind = std::get_if<hc::KindTarget>(&selector);
			for (const auto& target: targets | std::views::reverse) {
				if (named != nullptr) {
					if (!target.id.has_value() || target.id.value() != named->id) continue;
				} else if (by_kind != nullptr) {
					if (target.kind != by_kind->kind) continue;
				} else if (target.kind != hc::ControlFlowKind::While
				           && target.kind != hc::ControlFlowKind::For) {
					continue;
				}
				return &target;
			}
			return nullptr;
		}

		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			auto return_block = function.newBlock("return");
			auto return_scope = function.newScope(parent_scope);

			auto retrieve_value = return_block->addHole();

			// lower expr:
			auto expr_res = lowerExpr(*stmt.value, return_block, function, return_scope);

			auto possible_result = expr_res.getResultIfStored();

			if (possible_result.has_value() and !possible_result->isLocal()) {
				// Value is ready to return.
				retrieve_value.fillNop(return_scope);

			} else {
				// we need to store the result of the expression
				// in additional variable, so it doesn't get destroyed.

				// Retrieve type: if res is value It is local, otherwise only last instruction is
				// stored.
				auto res_type = possible_result.has_value() ? possible_result->get<MIRPlace>().type
				                                            : expr_res.getResultType();

				auto return_value = function.addReturnTmp(res_type);
				expr_res.storeResultInGivenPlace(
					MIRPlace(return_value), retrieve_value, {}, return_scope, {}
				);

				possible_result = return_value;
			}

			return_block->setTerminator(Instruction(
				Operation::ReturnValue,
				{},
				{ possible_result.value() },
				{},
				return_scope,
				{},
				{ stmt.getPosition() }
			));

			output({ expr_res.begin });
		}

		void visitVoidReturnStmt(const hc::VoidReturnStmt& stmt) override {
			auto return_block = function.newBlock("return.void");
			auto return_scope = function.newScope(parent_scope);
			return_block->setTerminator(
				{ Operation::ReturnVoid, {}, {}, {}, return_scope, {}, { stmt.getPosition() } }
			);
			output({ return_block });
		}

		void visitExprStmt(const hc::ExprStmt& stmt) override {
			auto expr_scope  = function.newScope(parent_scope);
			auto expr_result = lowerExpr(*stmt.expr, continuation, function, expr_scope);

			std::ignore = expr_result.getResult(function);
			output({ expr_result.begin });
		}

		void visitIfStmt(const hc::IfStmt& stmt) override {
			auto condition_scope      = function.newScope(parent_scope);
			auto condition_block      = function.newBlock("if.cond");
			auto get_condition_return = condition_block->addHole();
			auto lowered_condition
				= lowerExpr(*stmt.condition, condition_block, function, condition_scope);

			auto branch_targets = targets;
			branch_targets.push_back(ControlFlowTarget{
				.kind            = helios::code::ControlFlowKind::If,
				.id              = stmt.control_flow_id,
				.break_target    = continuation,
				.continue_target = lowered_condition.begin,
			});

			// I'm not sure if we need these scopes,
			// maybe we could just pass parent_scope as-is.
			// But this way it for sure works.
			auto then_scope = function.newScope(parent_scope);
			auto else_scope = function.newScope(parent_scope);

			auto else_block = function.newBlock("if.else");
			else_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, else_scope });
			auto else_body = lowerCodeBlockWithControlFlowTargets(
								 stmt.else_body, else_block, function, else_scope, branch_targets
			)
			                     .begin;

			auto then_block = function.newBlock("if.then");
			then_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, then_scope });
			auto then_body = lowerCodeBlockWithControlFlowTargets(
								 stmt.then_body, then_block, function, then_scope, branch_targets
			)
			                     .begin;

			auto possible_condition_res = lowered_condition.getResultIfStored();

			if (possible_condition_res.has_value() && !possible_condition_res->isLocal()) {
				get_condition_return.fillNop(condition_scope);

			} else {
				// Condition result must be stored in special temporary value, so we can use it
				// after the actual condition result is destroyed. Create extra temporary and assign
				// to it in-place or with extra move.
				possible_condition_res = function.addConditionTmp(condition_scope);

				lowered_condition.storeResultInGivenPlace(
					possible_condition_res->get<MIRPlace>(),
					get_condition_return,
					{
						flagConstruct(possible_condition_res->get<MIRPlace>().getBase<MIRLocalRef>()
				        ),
					},
					condition_scope,
					{}
				);
			}

			condition_block->setTerminator(Instruction{
				Operation::Branch,
				{},
				{
					*possible_condition_res,
					then_body->getID(),
					else_body->getID(),
				},
				{},
				condition_scope,
				{},
				{ stmt.getPosition() },
			});

			// Earlier statements may be inserted into this statement's entry block during
			// reverse lowering. Keep it separate from the condition used by continue if.
			auto entry_block = function.newBlock("if.entry");
			entry_block->setTerminator(
				{ Operation::Jump, {}, { lowered_condition.begin->getID() }, {}, parent_scope }
			);
			output({ entry_block });
		}

		void visitWhileStmt(const hc::WhileStmt& stmt) override {
			auto condition_scope = function.newScope(parent_scope);

			auto condition_continuation_block = function.newBlock("while.cond");

			auto get_condition_return = condition_continuation_block->addHole();

			auto expr_result = lowerExpr(
				*stmt.condition, condition_continuation_block, function, condition_scope
			);

			auto loop_scope = function.newScope(parent_scope);

			auto loop_continuation_block = function.newBlock("while.body.end");

			loop_continuation_block->setTerminator(
				{ Operation::Jump, {}, { expr_result.begin->getID() }, {}, loop_scope }
			);

			auto continue_target      = loop_continuation_block;
			auto body_statement_count = stmt.body.statements.size();
			if (stmt.control_flow_kind == hc::ControlFlowKind::For) {
				// The last desugared statement increments the index. A continue must run it.
				CORE_ASSERT(body_statement_count > 0, "Desugared for loop has no increment.");
				continue_target = lowerStmtWithControlFlowTargets(
									  *stmt.body.statements.back(),
									  loop_continuation_block,
									  function,
									  loop_scope,
									  targets
				)
				                      .begin;
				body_statement_count--;
			}

			auto body_targets = targets;
			body_targets.push_back(ControlFlowTarget{
				.kind            = stmt.control_flow_kind,
				.id              = stmt.control_flow_id,
				.break_target    = continuation,
				.continue_target = continue_target,
			});
			// Statements are lowered backwards and may be inserted into their continuation
			// block. Keep the continue target separate from that mutable continuation, or
			// a continue would also execute statements following it in the loop body.
			auto body_fallthrough = function.newBlock("while.body.fallthrough");
			body_fallthrough->setTerminator(
				{ Operation::Jump, {}, { continue_target->getID() }, {}, loop_scope }
			);
			StmtLowerRes loop_body{ body_fallthrough };
			for (usize i = body_statement_count; i > 0; --i) {
				loop_body = lowerStmtWithControlFlowTargets(
					*stmt.body.statements[i - 1], loop_body.begin, function, loop_scope, body_targets
				);
			}

			auto entry_block = function.newBlock("while.entry");

			entry_block->setTerminator(
				{ Operation::Jump, {}, { expr_result.begin->getID() }, {}, parent_scope }
			);

			auto possible_result = expr_result.getResultIfStored();

			if (possible_result.has_value() and !possible_result->isLocal()) {
				get_condition_return.fillNop(condition_scope);

			} else {
				possible_result = function.addConditionTmp(condition_scope);

				expr_result.storeResultInGivenPlace(
					possible_result->get<MIRPlace>(),
					get_condition_return,
					{ flagConstruct(possible_result->get<MIRPlace>().getBase<MIRLocalRef>()) },
					condition_scope,
					{}
				);
			}

			condition_continuation_block->setTerminator({
				Operation::Branch,
				{},
				{
					possible_result.value(),
					loop_body.begin->getID(),
					continuation->getID(),
				},
				{},
				condition_scope,
				{},
				{ stmt.getPosition() },
			});

			output({ entry_block });
		}

		void visitBreakStmt(const hc::BreakStmt& stmt) override {
			auto target = findTarget(stmt.target_selector);
			CORE_ASSERT(target != nullptr, "Break statement has no enclosing target.");

			auto break_block = function.newBlock("break");
			break_block->setTerminator({
				Operation::Jump,
				{},
				{ target->break_target->getID() },
				{},
				parent_scope,
				{},
				{ stmt.getPosition() },
			});
			output({ break_block });
		}

		void visitContinueStmt(const hc::ContinueStmt& stmt) override {
			auto target = findTarget(stmt.target_selector);
			CORE_ASSERT(target != nullptr, "Continue statement has no enclosing target.");

			auto continue_block = function.newBlock("continue");
			continue_block->setTerminator({
				Operation::Jump,
				{},
				{ target->continue_target->getID() },
				{},
				parent_scope,
				{},
				{ stmt.getPosition() },
			});
			output({ continue_block });
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			auto optional_local = function.findLocal(stmt.helios_symbol);

			CORE_ASSERT(
				optional_local.has_value(),
				"Variable statement refers to local variable that is not defined in the function."
			);

			auto local = optional_local.value();
			// we set the lifetime scope of the local variable here
			// since we only know it here:
			local->setLifetimeScope(parent_scope);

			auto local_construction_hole = continuation->addHole();
			auto assignment_scope        = function.newScope(parent_scope);
			auto expr_result
				= lowerExpr(*stmt.initial_value, continuation, function, assignment_scope);

			expr_result.storeResultInGivenPlace(
				MIRPlace(local),
				local_construction_hole,
				{ flagConstruct(local) },
				assignment_scope,
				{ stmt.getPosition() }
			);
			output({ expr_result.begin });
			return;
		}

		void visitAssignmentStmt(const hc::AssignmentStmt& stmt) override {
			auto assignment_scope = function.newScope(parent_scope);

			auto target_construction_hole = continuation->addHole();

			// This is for cases like
			// a = foo(&a)
			// Where the a destructor will be inserted, but we don't know if `foo` uses `a`.
			// In that case we should make a temporary result and then assign to the output.
			// tmp = foo(&a)
			// destruct(a) <- added in the later pass
			// a = tmp
			bool destructor_in_between
				= not stmt.location_expr->expression_type.getSymbolType().isTriviallyDestructible(
					function.getContext()
				);
			base::Optional<BlockBuilder::InstructionHole> second_hole;
			if (destructor_in_between) second_hole = continuation->addHole();

			auto right_result
				= lowerExpr(*stmt.new_value_expr, continuation, function, assignment_scope);

			auto left_result
				= lowerExpr(*stmt.location_expr, right_result.begin, function, assignment_scope);

			auto left_val = left_result.getResult(function);

			CORE_ASSERT(
				left_val.isLocal() || left_val.isGlobal(),
				"Left side of assignment statement doesn't contain reference to local "
				"variable or "
				"a global variable."
			);


			variant_match(left_val.getVariant()) {
				variant_case(MIRPlace, place) {
					// Reinitialize the whole local: marks it alive again for liveness (e.g.
					// after a move-out). Only for a bare local target — a projected store (`x.p
					// = ...`, `aa[i] = ...`) writes a sub-place and does not change the
					// whole-local liveness.
					std::vector<OperationFlag> flags;
					if (place.isLocal() && place.projection_chain.empty())
						flags.push_back(flagReinit(place.getBase<MIRLocalRef>()));

					// We want to cover cases like a = a
					if (destructor_in_between) {
						auto tmp = function.addTmp(place.type, assignment_scope);
						right_result.storeResultInGivenPlace(
							MIRPlace(tmp),
							second_hole.value(),
							{ flagConstruct(tmp) },
							assignment_scope,
							{ stmt.getPosition() }
						);
						auto tmp_result = ExprLowerRes(right_result.begin, tmp);
						flags.push_back(flagMove(tmp));

						tmp_result.storeResultInGivenPlace(
							place,
							target_construction_hole,
							flags,
							assignment_scope,
							{ stmt.getPosition() }
						);
					} else {
						right_result.storeResultInGivenPlace(
							place,
							target_construction_hole,
							flags,
							assignment_scope,
							{ stmt.getPosition() }
						);
					}
					output({ left_result.begin });
				}
				variant_default { CORE_PANIC("Assignment to unsupported MIRValue kind."); }
			}
		}

		void visitBlockStmt(const hc::BlockStmt& stmt) override {
			auto                            block_scope  = function.newScope(parent_scope);
			auto                            body_targets = targets;
			base::Optional<BlockBuilderRef> entry;
			auto                            body_continuation = continuation;
			if (stmt.control_flow_kind.has_value()) {
				entry = function.newBlock(
					stmt.control_flow_kind.value() == hc::ControlFlowKind::If ? "const.if.entry"
																			  : "block.entry"
				);
				// Body statements are lowered into their continuation. Do not allow them to
				// modify the destination of break from this block.
				body_continuation = function.newBlock("block.body.fallthrough");
				body_continuation->setTerminator(
					{ Operation::Jump, {}, { continuation->getID() }, {}, block_scope }
				);
				body_targets.push_back(ControlFlowTarget{
					.kind            = stmt.control_flow_kind.value(),
					.id              = stmt.control_flow_id,
					.break_target    = continuation,
					.continue_target = entry.value(),
				});
			}

			auto block_body = lowerCodeBlockWithControlFlowTargets(
				stmt.body, body_continuation, function, block_scope, std::move(body_targets)
			);
			if (entry.has_value()) {
				entry.value()->setTerminator(
					{ Operation::Jump, {}, { block_body.begin->getID() }, {}, parent_scope }
				);
				// Keep the entry used by continue block free of instructions from the
				// preceding statement, which will be lowered into our returned block.
				auto outer_entry = function.newBlock("block.outer.entry");
				outer_entry->setTerminator(
					{ Operation::Jump, {}, { entry.value()->getID() }, {}, parent_scope }
				);
				output({ outer_entry });
			} else {
				output({ block_body.begin });
			}
		}
	};

	namespace {
		StmtLowerRes lowerStmtWithControlFlowTargets(
			const hc::Stmt&    stmt,
			BlockBuilderRef    continuation,
			FunctionBuilder&   function,
			ScopeRef           parent_scope,
			ControlFlowTargets targets
		) {
			StmtBlockVisitor visitor{ continuation, function, parent_scope, std::move(targets) };
			stmt.acceptVisitor(visitor);
			return visitor.out.value();
		}

		StmtLowerRes lowerCodeBlockWithControlFlowTargets(
			const hc::CodeBlock& code_block,
			BlockBuilderRef      continuation,
			FunctionBuilder&     function,
			ScopeRef             parent_scope,
			ControlFlowTargets   targets
		) {
			StmtLowerRes last_result{ continuation };

			for (auto& stmt: code_block.statements | std::views::reverse) {
				last_result = lowerStmtWithControlFlowTargets(
					*stmt, continuation, function, parent_scope, targets
				);
				continuation = last_result.begin;
			}
			return last_result;
		}
	}

	StmtLowerRes lowerStmt(
		const hc::Stmt&  stmt,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         parent_scope
	) {
		return lowerStmtWithControlFlowTargets(stmt, continuation, function, parent_scope, {});
	}

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	) {
		return lowerCodeBlockWithControlFlowTargets(
			code_block, continuation, function, parent_scope, {}
		);
	}
}
