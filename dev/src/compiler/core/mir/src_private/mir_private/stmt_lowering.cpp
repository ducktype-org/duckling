#include "stmt_lowering.hpp"

#include "expr_lowering.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/collections/optional.hpp>

#include <ranges>

namespace compiler::mir {

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

		StmtBlockVisitor(
			BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
		):
			  continuation(continuation),
			  function(function),
			  parent_scope(parent_scope) {}

		base::Optional<StmtLowerRes> out;

		void output(const StmtLowerRes& value) {
			CORE_ASSERT(this->out.empty(), "Output already set.");
			this->out.emplace(value);
		}

		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			auto return_block = function.newBlock();
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
			auto return_block = function.newBlock();
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
			auto condition_scope = function.newScope(parent_scope);

			// I'm not sure if we need these scopes,
			// maybe we could just pass parent_scope as-is.
			// But this way it for sure works.
			auto then_scope = function.newScope(parent_scope);
			auto else_scope = function.newScope(parent_scope);

			auto else_block = function.newBlock();
			else_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, else_scope });
			auto else_body = lowerCodeBlock(stmt.else_body, else_block, function, else_scope).begin;

			auto then_block = function.newBlock();
			then_block->setTerminator(Instruction{
				Operation::Jump, {}, { continuation->getID() }, {}, then_scope });
			auto then_body = lowerCodeBlock(stmt.then_body, then_block, function, then_scope).begin;

			auto condition_block = function.newBlock();

			auto get_condition_return = condition_block->addHole();

			auto lowered_condition
				= lowerExpr(*stmt.condition, condition_block, function, condition_scope);

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

			output({ lowered_condition.begin });
		}

		void visitWhileStmt(const hc::WhileStmt& stmt) override {
			auto condition_scope = function.newScope(parent_scope);

			auto condition_continuation_block = function.newBlock();

			auto get_condition_return = condition_continuation_block->addHole();

			auto expr_result = lowerExpr(
				*stmt.condition, condition_continuation_block, function, condition_scope
			);

			auto loop_scope = function.newScope(parent_scope);

			auto loop_continuation_block = function.newBlock();

			loop_continuation_block->setTerminator(
				{ Operation::Jump, {}, { expr_result.begin->getID() }, {}, loop_scope }
			);

			auto loop_body
				= lowerCodeBlock(stmt.body, loop_continuation_block, function, loop_scope);

			auto entry_block = function.newBlock();

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
			// @TODO: #448 Search for location in global scope as well.
			auto assignment_scope = function.newScope(parent_scope);

			auto target_construction_hole = continuation->addHole();

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
					// Reinitialize the whole local: marks it alive again for liveness (e.g. after a
					// move-out). Only for a bare local target — a projected store (`x.p = ...`,
					// `aa[i] = ...`) writes a sub-place and does not change the whole-local liveness.
					std::vector<OperationFlag> flags;
					if (place.isLocal() && place.projection_chain.empty())
						flags.push_back(flagReinit(place.getBase<MIRLocalRef>()));

					right_result.storeResultInGivenPlace(
						place,
						target_construction_hole,
						flags,
						assignment_scope,
						{ stmt.getPosition() }
					);
					output({ left_result.begin });
				}
				variant_default { CORE_PANIC("Assignment to unsupported MIRValue kind."); }
			}
		}

		void visitBlockStmt(const hc::BlockStmt& stmt) override {
			auto block_scope = function.newScope(parent_scope);

			auto block_body = lowerCodeBlock(stmt.body, continuation, function, block_scope);

			output({ block_body.begin });
		}
	};

	StmtLowerRes lowerStmt(
		const hc::Stmt&  stmt,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         parent_scope
	) {
		StmtBlockVisitor visitor{ continuation, function, parent_scope };
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	) {
		StmtLowerRes last_result{ continuation };

		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result  = lowerStmt(*stmt, continuation, function, parent_scope);
			continuation = last_result.begin;
		}
		return last_result;
	}
}
