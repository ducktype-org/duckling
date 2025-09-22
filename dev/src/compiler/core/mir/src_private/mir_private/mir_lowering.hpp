#pragma once 

#include "mir_lowering.hpp"

#include "mir_builders.hpp"
#include <mir/mir_lowering/mir_lifetimes.hpp>
#include <mir/mir_lowering/mir_validation.hpp>

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <helios/hout/hout.hpp>

#include <base/exceptions.hpp>
#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <base/variant.hpp>

#include <query_framework/query_impl.hpp>
#include <query_framework/query_result.hpp>

#include <ranges>
#include <stack>
#include <unordered_set>
#include <variant>

namespace compiler::mir {
	namespace hc = helios::code;

	/**
	 * @brief Represents result of statement lowering, which is
	 * a BlockBuilderRef that is the beginning of the lowered statement.
	 */
	struct StmtLowerRes final {
		BlockBuilderRef begin;
	};
	
	/**
	 * @brief Represents a partial result of expression lowering.
	 *
	 * This consists of a BlockBuilderRef marking the beginning of the lowered
	 * expression and either:
	 *  - a MIRValue holding the result of the expression, OR
	 *  - a Finalizer representing the last instruction that saves the result
	 *    without specifying its target.
	 *
	 * For complete lowering, call the dedicated function that stores the result
	 * in the desired location while (if possible) avoiding the creation of unnecessary temporaries.
	 *
	 * In most cases, use getResult() or storeResultInGivenVariable().
	 */
	struct ExprLowerRes final {
		BlockBuilderRef begin;

		/**
		 * @brief Represents a finalizer instruction that saves the result of an expression.
		 * Stores hole where instruction will be saved, instruction without output and type of
		 * result. This instruction can be performed on provided varaible
		 * (storeResultInGivenVariable) or generated temporary (getResult).
		 */
		struct Finalizer final {
			BlockBuilder::InstructionHole hole;
			Instruction                   instr;
			tsh::SymbolType<>             type;
		};

		std::variant<MIRValue, Finalizer> value;

		ExprLowerRes(BlockBuilderRef begin, std::variant<MIRValue, Finalizer> value):
			  begin{ begin },
			  value{ std::move(value) } {}

		/**
		 * @brief helper function returing type of result. Can be used if MIRValue is not stored.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getResultType() {
			variant_match(value) {
				variant_case(Finalizer, res_data) { return res_data.type; }
				variant_default {
					CORE_PANIC("Function can be run only if MIRValue is not stored");
				}
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Helper function that returns MIRvalue if it is already stored in structure.
		 */
		[[nodiscard]]
		base::Optional<MIRValue> getResultIfStored() {
			variant_match(value) {
				variant_case(MIRValue, val) { return val; }
				variant_case_novalue(Finalizer) { return std::nullopt; }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief If result of expr is value already returns it,
		 * Otherwise creates temporary, makes last instruction save res there and returns it.
		 * @note may use InstructionHole stored in structure, probably use only once.
		 */
		[[nodiscard]]
		MIRValue getResult(FunctionBuilder& function) {
			variant_match(value) {
				variant_case(MIRValue, val) { return val; }
				variant_case(Finalizer, res_data) {
					auto result = function.addTmp(res_data.type, res_data.instr.scope);
					res_data.instr.output.emplace(result);
					res_data.instr.flags.push_back(flagConstruct(result));
					res_data.hole.fill(res_data.instr);
					value = result;
					return result;
				}
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief If result of expr is value it creates
		 * instruction that will assign result to it. Otherwise it makes the last instruction of the
		 * expression save result directly to the target.
		 * @note may use InstructionHole stored in stucture, probably use only once.
		 */
		void storeResultInGivenVariable(
			const std::variant<LocalRef, MirGlobal>& target,
			BlockBuilder::InstructionHole&           hole,
			const std::vector<OperationFlag>&        flags,
			ScopeRef                                 scope
		) {
			variant_match(value) {
				variant_case(MIRValue, val) {
					hole.fill(Instruction{
						Operation::Assign,
						target,
						{ val },
						flags,
						scope,
					});
				}
				variant_case(Finalizer, res_data) {
					CORE_ASSERT(scope == res_data.instr.scope, "Scope mismatch!");

					hole.fillNop(scope);
					std::visit([&](auto&& val) { res_data.instr.output.emplace(val); }, target);
					res_data.hole.fill(res_data.instr);
					res_data.instr.flags.insert(
						res_data.instr.flags.end(), flags.begin(), flags.end()
					);
					std::visit([&](auto&& val) { value = val; }, target);
				}
			}
		}
	};

	/**
	 * @brief Lowers statement.
	 *
	 * @param stmt
	 * @param continuation Block that should be executed after this statement.
	 * @param function Function that we are lowering this statement in.
	 * @param parent_scope Scope of the parent of this Statement.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerStmt(
		const hc::Stmt&  stmt,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         parent_scope
	);

	/**
	 * @brief Lowers expression.
	 *
	 * @param expr
	 * @param continuation Block that should be executed after this expression.
	 * @param function Function that we are lowering this expression in.
	 * @param expr_scope Lifetime Scope this expression should be in.
	 * @return ExprLowerRes
	 */
	ExprLowerRes lowerExpr(
		const hc::Expr&  expr,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         expr_scope
	);

	/**
	 * @brief Lowers code-block, by lowering all statements in the block.
	 *
	 * @param code_block
	 * @param continuation Block that should be executed after this code block.
	 * @param function Function that we are lowering this code block in.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	);

	/**
	 * @brief Visitor that collects all local variables in the function and adds them directly
	 * to the FunctionBuilder. It sets variable scopes for parameters, but doesn't set it for
	 * other local variables. Scope of other local variables is set when visiting VariableStmt
	 * in StmtBlockVisitor, since only then is the scope of the variable known.
	 */
	struct LocalVarCollectionVisitor: public hc::HoutStmtVisitorPanicky {
		FunctionBuilder& function;

		LocalVarCollectionVisitor(FunctionBuilder& function): function(function) {}

		/**
		 * Helper function that recursively goes over the code block and collects all local
		 * variables.
		 */
		void goOverCodeBlock(const hc::CodeBlock& code_block) {
			for (const auto& stmt: code_block.statements) stmt->acceptVisitor(*this);
		}

		/**
		 * @brief Collects all local variables in the function and adds them directly to the
		 * FunctionBuilder.
		 */
		void collect(const helios::HOUTFunction& hout_function) {
			auto function_helios_symbol = function.getHeliosSymbol();
			variant_match(function_helios_symbol) {
				variant_case(FunctionSymID, function_sym) {
					CORE_ASSERT(
						function_sym.id == hout_function.original_symbol,
						"Bad function passed to LocalVarCollectionVisitor"
					);
				}

				variant_default {
					CORE_PANIC(
						"The Function wasn't created from HOUTFunction, so you should not use "
						"collect."
					);
				}
			}

			u64 parameter_index = 0;
			for (const auto& parameter: *hout_function.content.parameters) {
				auto local = function.addParameter(parameter.helios_symbol, parameter_index);
				local->setLifetimeScope(function.getTopLevelScope());
				parameter_index++;
			}
			goOverCodeBlock(*hout_function.content.body);
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			function.addLocal(stmt.helios_symbol);
		}

		void visitIfStmt(const hc::IfStmt& stmt) override {
			goOverCodeBlock(stmt.then_body);
			goOverCodeBlock(stmt.else_body);
		}

		void visitWhileStmt(const hc::WhileStmt& stmt) override { goOverCodeBlock(stmt.body); }

		// Explicit empty boilerplate. Expected changes when block expressions are implemented.

		void visitReturnStmt(const hc::ReturnStmt&) override {}

		void visitVoidReturnStmt(const hc::VoidReturnStmt&) override {}

		void visitExprStmt(const hc::ExprStmt&) override {}

		void visitAssignmentStmt(const hc::AssignmentStmt&) override {}
	};

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

		void output(StmtLowerRes value) {
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
				auto res_type = possible_result.has_value() ? possible_result->get<LocalRef>()->type
				                                            : expr_res.getResultType();

				auto return_value = function.addNoLifetimeTmp(res_type);

				// Set move flag only if value exists.
				std::vector<OperationFlag> flags = { flagConstruct(return_value) };
				if (possible_result.has_value())
					flags.push_back(flagMove(possible_result->get<LocalRef>()));


				expr_res.storeResultInGivenVariable(
					return_value, retrieve_value, flags, return_scope
				);

				possible_result = return_value;
			}

			return_block->setTerminator(Instruction(
				Operation::ReturnValue, {}, { possible_result.value() }, {}, return_scope
			));

			output({ expr_res.begin });
		}

		void visitVoidReturnStmt(const hc::VoidReturnStmt&) override {
			auto return_block = function.newBlock();
			auto return_scope = function.newScope(parent_scope);
			return_block->setTerminator({ Operation::ReturnVoid, {}, {}, {}, return_scope });
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
			else_block->setTerminator(
				Instruction{ Operation::Jump, {}, { continuation->getID() }, {}, else_scope }
			);
			auto else_body = lowerCodeBlock(stmt.else_body, else_block, function, else_scope).begin;

			auto then_block = function.newBlock();
			then_block->setTerminator(
				Instruction{ Operation::Jump, {}, { continuation->getID() }, {}, then_scope }
			);
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
				possible_condition_res = function.addNoLifetimeBoolTmp();

				lowered_condition.storeResultInGivenVariable(
					possible_condition_res->get<LocalRef>(),
					get_condition_return,
					{ flagConstruct(possible_condition_res->get<LocalRef>()) },
					condition_scope
				);
			}

			condition_block->setTerminator(
				Instruction{
					Operation::Branch,
					{},
					{ *possible_condition_res, then_body->getID(), else_body->getID() },
					{},
					condition_scope,
				}
			);

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
				possible_result = function.addNoLifetimeBoolTmp();

				expr_result.storeResultInGivenVariable(
					possible_result->get<LocalRef>(),
					get_condition_return,
					{ flagConstruct(possible_result->get<LocalRef>()) },
					condition_scope
				);
			}

			condition_continuation_block->setTerminator(
				{
					Operation::Branch,
					{},
					{ possible_result.value(), loop_body.begin->getID(), continuation->getID() },
					{},
					condition_scope,
				}
			);

			output({ entry_block });
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			auto optional_local = function.findLocal(stmt.helios_symbol);

			CORE_ASSERT(
				optional_local.has_value(),
				"Variable statement refers to local variable that is not defined in the "
				"function."
			);

			auto local = optional_local.value();
			// we set the lifetime scope of the local variable here
			// since we only know it here:
			local->setLifetimeScope(parent_scope);

			auto local_construction_hole = continuation->addHole();

			match_optional(stmt.initial_value) {
				opt_some(value) {
					auto assignment_scope = function.newScope(parent_scope);
					auto expr_result = lowerExpr(*value, continuation, function, assignment_scope);

					expr_result.storeResultInGivenVariable(
						local, local_construction_hole, { flagConstruct(local) }, assignment_scope
					);
					output({ expr_result.begin });
					return;
				}
				opt_none { throw base::NotYetImplemented("variable without initial value in MIR"); }
			}

			CORE_UNREACHABLE();
		}

		void visitAssignmentStmt(const hc::AssignmentStmt& stmt) override {
			// TODO: #448 Search for location in global scope as well.
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

			std::visit(
				[&](auto&& ref) {
					using T = std::decay_t<decltype(ref)>;
					if constexpr (std::is_same_v<T, LocalRef> || std::is_same_v<T, MirGlobal>) {
						right_result.storeResultInGivenVariable(
							ref, target_construction_hole, {}, assignment_scope
						);
						output({ left_result.begin });
					} else {
						CORE_PANIC("Assignment to unsupported MIRValue type");
					}
				},
				left_val.getVariant()
			);
		}
	};

	/**
	 * @brief Visitor that implements actual logic of lowering expression.
	 * @note The result of the visitor is stored in out member. To store expr
	 * result somewhere, call finalize with place to store it
	 */
	struct ExprBlockVisitor final: public hc::HoutExprVisitor {
		BlockBuilderRef continuation;

		base::Optional<ExprLowerRes> out;

		FunctionBuilder& function;

		/**
		 * The scope of the expression, where it and its result should live in.
		 */
		ScopeRef expr_scope;

		ExprBlockVisitor(
			BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
		):
			  continuation(continuation),
			  function(function),
			  expr_scope(expr_scope) {}

		void output(ExprLowerRes lowering_result) {
			CORE_ASSERT(out.empty(), "Output already set.");
			out.emplace(lowering_result);
		}

		void valueOutput(BlockBuilderRef begin, const MIRValue& value) {
			CORE_ASSERT(out.empty(), "Output already set.");
			out.emplace(ExprLowerRes(begin, value));
		}

		void noValueOutput(
			BlockBuilderRef                      begin,
			const BlockBuilder::InstructionHole& hole,
			const Instruction&                   instr,
			const tsh::SymbolType<>&             type
		) {
			CORE_ASSERT(out.empty(), "Output already set.");
			CORE_ASSERT(instr.output.empty(), "instruction shouldn't have output set.");
			out.emplace(ExprLowerRes(begin, ExprLowerRes::Finalizer(hole, instr, type)));
		}

		void visitLiteralIntExpr(const hc::LiteralIntExpr& expr) override {
			valueOutput(continuation, MIRValue{ MirIntegerConst{ expr.value } });
		}

		void visitLiteralBoolExpr(const hc::LiteralBoolExpr& expr) override {
			valueOutput(continuation, MIRValue{ MirBoolConst{ expr.value } });
		}

		void visitLiteralStringExpr(const hc::LiteralStringExpr&) override {
			throw base::NotYetImplemented("string literal");
		}

		void visitLiteralTypeExpr(const hc::LiteralTypeExpr&) override {
			throw base::NotYetImplemented("type literal");
		}

		void visitIdentifierExpr(const hc::IdentifierExpr& expr) override {
			auto optional_local = function.findLocal(expr.symbol);

			if (optional_local.has_value()) {
				valueOutput(continuation, MIRValue{ optional_local.value() });
			} else {
				//@TODO: chack if the symbol is a real global variable.
				valueOutput(
					continuation,
					MIRValue{ MirGlobal({ expr.symbol, expr.expression_type.getSymbolType() }) }
				);
			}
		}

		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto target_construction_hole = continuation->addHole();

			auto       lowered_right = lowerExpr(*expr.rhs, continuation, function, expr_scope);
			const auto res_right     = lowered_right.getResult(function);
			auto lowered_left   = lowerExpr(*expr.lhs, lowered_right.begin, function, expr_scope);
			const auto res_left = lowered_left.getResult(function);

			// Fill the hole with the binary operation.
			// Assume (for now?) that the arguments are of the same type,
			// and the result is of the same type as the arguments.
			const auto argument_type       = locationType(res_right, function.getContext());
			const auto other_argument_type = locationType(res_left, function.getContext());
			CORE_ASSERT(
				argument_type.getType() == other_argument_type.getType(),
				"Binary operator with different argument types"
			);
			const auto      result_type = expr.expression_type.getSymbolType();
			const Operation operation   = builtinBinaryToOperation(expr.operation);

			noValueOutput(
				lowered_left.begin,
				target_construction_hole,
				Instruction(operation, {}, { res_left, res_right }, {}, expr_scope),
				result_type
			);
		}

		void visitUnaryOperatorExpr(const hc::UnaryOperatorExpr& expr) override {
			// Construct the result of the expression in reverse.
			auto       target_construction_hole = continuation->addHole();
			auto       lowered     = lowerExpr(*expr.expr, continuation, function, expr_scope);
			const auto res_lowered = lowered.getResult(function);

			const auto result_type = expr.expression_type.getSymbolType();

			const Operation operation = builtinUnaryToOperation(expr.operation);

			noValueOutput(
				lowered.begin,
				target_construction_hole,
				Instruction(operation, {}, { res_lowered }, {}, expr_scope),
				result_type
			);
		}

		void visitTernaryOperatorExpr(
			const helios::code::TernaryOperatorExpr& ternary_expr
		) override {
			// Get info about the target.
			const auto result_type     = ternary_expr.expression_type.getSymbolType();
			const auto target_location = function.addTmp(result_type, expr_scope);

			auto build_case_block = [this, &target_location](hc::Expr& case_expr) {
				auto block = function.newBlock();
				block->setTerminator(
					{ Operation::Jump, {}, { continuation->getID() }, {}, expr_scope }
				);
				auto assign_hole = block->addHole();

				auto lowered_block = lowerExpr(case_expr, block, function, expr_scope);

				lowered_block.storeResultInGivenVariable(
					target_location, assign_hole, { flagConstruct(target_location) }, expr_scope
				);


				return lowered_block.begin;
			};

			auto else_block = build_case_block(*ternary_expr.if_false);
			auto then_block = build_case_block(*ternary_expr.if_true);

			// Build branching.
			auto condition_block = function.newBlock();
			auto lowered_condition
				= lowerExpr(*ternary_expr.condition, condition_block, function, expr_scope);


			condition_block->setTerminator(
				{
					Operation::Branch,
					{},
					{ lowered_condition.getResult(function),
			          then_block->getID(),
			          else_block->getID() },
					{},
					expr_scope,
				}
			);

			// Return (always value).
			valueOutput(lowered_condition.begin, target_location);
		}

		void visitParenthesisExpr(const hc::ParenthesisExpr& expr) override {
			output(lowerExpr(*expr.inner, continuation, function, expr_scope));
		}

		void visitTupleTypeConstructorExpr(const hc::TupleTypeConstructorExpr&) override {
			throw base::NotYetImplemented("tuple constructor");
		}

		void visitVariantTypeConstructorExpr(const hc::VariantTypeConstructorExpr&) override {
			throw base::NotYetImplemented("variant constructor");
		}

		void visitAccessExpr(const hc::AccessExpr&) override {
			throw base::NotYetImplemented("access expr lowering");
		}

		void visitSequenceExpr(const hc::SequenceExpr&) override {
			throw base::NotYetImplemented("sequence expr lowering");
		}

		void visitChainComparisonExpr(const hc::ChainComparisonExpr& chain_expr) override {
			CORE_ASSERT(!chain_expr.expressions.empty(), "Empty chain comparison");
			CORE_ASSERT(chain_expr.expressions.size() != 1, "Single element chain comparison");
			CORE_ASSERT(
				chain_expr.operators.size() == chain_expr.expressions.size() - 1,
				"Operands: " + std::to_string(chain_expr.operators.size())
					+ " expressions: " + std::to_string(chain_expr.expressions.size())
					+ ", but expected one less operator then expression."
			);

			auto lower_subexpr_with_result
				= [this](CRef<hc::Expr> expression, BlockBuilderRef next_block) {
					  auto lowered = lowerExpr(*expression, next_block, function, expr_scope);
					  return std::pair{ lowered.begin, lowered.getResult(function) };
				  };

			using namespace std::views;

			// Place for a comparison instruction
			auto last_comparison_block = function.newBlock();
			auto prev_cmp_hole         = last_comparison_block->addHole();

			// After the last comparison, continue regardless of the result.
			last_comparison_block->setTerminator(
				Instruction{ Operation::Jump, {}, { continuation->getID() }, {}, expr_scope }
			);

			// The result of evaluating the expression (result of the last evaluated sub-expression).
			auto boolean_output
				= function.addTmp(chain_expr.expression_type.getSymbolType(), expr_scope);

			// The left-over value. We mantain that this has to partake in only one comparison,
			// which will be placed in prev_cmp_hole.boolean
			auto [prev_block, prev_value] = lower_subexpr_with_result(
				chain_expr.expressions.back().ref(), last_comparison_block
			);

			auto mir_operators = chain_expr.operators | transform(builtinBinaryToOperation);

			// First and last expressions require special handling. We build them in reverse, as usual.
			auto expressions = chain_expr.expressions | drop(1) | reverse | drop(1);
			auto comparisons = mir_operators | drop(1) | reverse;

			for (const auto& [expr, comp]: zip(expressions, comparisons)) {
				// Place for the next comparison.
				BlockBuilderRef new_comparison_block = function.newBlock();
				auto            new_cmp_hole         = new_comparison_block->addHole();
				new_comparison_block->setTerminator(
					Instruction{ Operation::Branch,
				                 {},
				                 { boolean_output, prev_block->getID(), continuation->getID() },
				                 {},
				                 expr_scope }
				);  // We exaluate prev_value only after this comparison is true, as
				    // prev_cmp will be the first comparison it is a part of.

				// Next expression (completes the prev_cmp).
				auto [new_block, new_value]
					= lower_subexpr_with_result(expr.ref(), new_comparison_block);

				// We create the prev_cmp, as we only now have both expressions.
				prev_cmp_hole.fill(
					Instruction{
						comp, { boolean_output }, { new_value, prev_value }, {}, expr_scope }
				);

				prev_block    = new_block;
				prev_cmp_hole = new_cmp_hole;

				// expr_result participated in the previous comparion fulfilling the invariant.
				prev_value = new_value;
			}

			// The first expression to be evaluated.
			auto [first_block, first_value]
				= lower_subexpr_with_result(chain_expr.expressions.front().ref(), prev_block);

			// The first comparison to be performed.
			prev_cmp_hole.fill(
				Instruction{ mir_operators.front(),
			                 { boolean_output },
			                 { first_value, prev_value },
			                 { flagConstruct(boolean_output) },
			                 expr_scope }
			);

			valueOutput(first_block, boolean_output);
		}

		void visitCallExpr(const hc::CallExpr& expr) override {
			auto call = continuation->addHole();

			auto                  sub_continuation = continuation;
			std::vector<MIRValue> args;

			auto function_symid = helios::getIdentifierExprSymID(expr.callee.ref());
			if (not function_symid.has_value()) {
				CORE_PANIC(
					"Call expression where callee is not an identifier expression is currently not "
					"supported."
				);
			}
			args.emplace_back(MirFunctionLiteral{ function_symid.value() });
			for (const auto& arg: expr.arguments) {
				auto arg_lowered = lowerExpr(*arg, sub_continuation, function, expr_scope);

				args.push_back(arg_lowered.getResult(function));
				sub_continuation = arg_lowered.begin;
			}

			// @TODO: #505 here in the future we (probably) will have to handle
			// move operations related to the passing of the arguments to the function

			return noValueOutput(
				sub_continuation,
				call,
				Instruction{
					Operation::Call,
					{},
					args,
					{},
					expr_scope,
				},
				expr.expression_type.getSymbolType()
			);
		}

	private:
		static Operation builtinBinaryToOperation(const hc::BuiltinBinary builtin) {
			using enum hc::BuiltinBinary;
			switch (builtin) {
			case IntegerAdd:
				return Operation::IntegerAdd;
			case IntegerSub:
				return Operation::IntegerSub;
			case IntegerMul:
				return Operation::IntegerMul;
			case IntegerDiv:
				return Operation::IntegerDiv;
			case IntegerMod:
				return Operation::IntegerMod;
			case IntegerPow:
				// @fixme: Implement exponentiation as a function call.
				throw base::NotYetImplemented("Exponentiation on variables");
			case IntegerLt:
				return Operation::IntegerLt;
			case IntegerGt:
				return Operation::IntegerGt;
			case IntegerLteq:
				return Operation::IntegerLteq;
			case IntegerGteq:
				return Operation::IntegerGteq;
			case IntegerEq:
				return Operation::IntegerEq;
			case IntegerNeq:
				return Operation::IntegerNeq;
			case BooleanAnd:
				return Operation::BooleanAnd;
			case BooleanOr:
				return Operation::BooleanOr;
			default:
				CORE_UNREACHABLE();
			}
		}

		static Operation builtinUnaryToOperation(const hc::BuiltinUnary builtin) {
			using enum hc::BuiltinUnary;
			switch (builtin) {
			case IntegerNegation:
				return Operation::IntegerNeg;
			case BooleanNot:
				return Operation::BooleanNot;
			default:
				CORE_UNREACHABLE();
			}
		}

		/**
		 * Get the type of a location, assuming that it is a local value.
		 * @param location A MIR location which holds a local value.
		 * @param ctx The query context for AbstractType generation.
		 * @return The type of the local value.
		 */
		static tsh::SymbolType<> locationType(const MIRValue location, query::Context& ctx) {
			variant_match(location.getVariant()) {
				variant_case_novalue(MirIntegerConst) {
					return tsh::SymbolType<>{
						ctx.query<tsh::QueryIntegralType>({ 64 }),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case_novalue(MirBoolConst) {
					return tsh::SymbolType<>{
						ctx.query<tsh::QueryBoolType>({}),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(LocalRef, local) { return local->type; }
				variant_case(MirGlobal, global) { return global.type; }
				variant_default { CORE_UNREACHABLE(); }
			}
			CORE_UNREACHABLE();
		}
	};

	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
