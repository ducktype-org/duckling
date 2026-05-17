#include "mir_queries.hpp"

#include "../mir_structure/mir_structure.hpp"
#include "mir_lifetimes.hpp"
#include "mir_validation.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir_private/expr_lowering.hpp>
#include <mir_private/mir_builders.hpp>
#include <mir_private/stmt_lowering.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <stack>
#include <unordered_set>

namespace compiler::mir {
	namespace hc = helios::code;

	u64 KeyOf_LowerToMIRFunction::queryUnstablePerfectHash() const {
		return function->queryUnstablePerfectHash();
	}

	u64 KeyOf_LowerGlobalDataToMIRFunction::queryUnstablePerfectHash() const {
		return global_data->helios_symbol.queryUnstablePerfectHash();
	}

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
			// This order is important for the correct order of the destructors and
			// scoping flags.
			for (const auto& stmt: code_block.statements) stmt->acceptVisitor(*this);
		}

		/**
		 * @brief Collects all local variables in the function and adds them directly to the
		 * FunctionBuilder.
		 */
		void collect(CRef<helios::HOUTFunction> hout_function) {
			auto function_helios_symbol = function.getHeliosSymbol();
			variant_match(function_helios_symbol) {
				variant_case(FunctionSymID, function_sym) {
					CORE_ASSERT(
						function_sym.id == hout_function->declaration->original_symbol,
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
			for (const auto& parameter: hout_function->declaration->parameters) {
				auto local = function.addParameter(parameter.helios_symbol, parameter_index);
				local->setLifetimeScope(function.getTopLevelScope());
				parameter_index++;
			}
			goOverCodeBlock(*hout_function->body);
		}

		void visitVariableStmt(const hc::VariableStmt& stmt) override {
			function.addLocal(stmt.helios_symbol);
		}

		void visitIfStmt(const hc::IfStmt& stmt) override {
			goOverCodeBlock(stmt.then_body);
			goOverCodeBlock(stmt.else_body);
		}

		void visitWhileStmt(const hc::WhileStmt& stmt) override { goOverCodeBlock(stmt.body); }

		void visitBlockStmt(const hc::BlockStmt& stmt) override { goOverCodeBlock(stmt.body); }

		// Explicit empty boilerplate. Expected changes when block expressions are implemented.

		void visitReturnStmt(const hc::ReturnStmt&) override {}

		void visitVoidReturnStmt(const hc::VoidReturnStmt&) override {}

		void visitExprStmt(const hc::ExprStmt&) override {}

		void visitAssignmentStmt(const hc::AssignmentStmt&) override {}
	};

	const Function& lowerToPreMIRFunction(query::Context& ctx, CRef<helios::HOUTFunction> function) {
        auto res = ctx.query<LowerToPreMIRFunction>(function->declaration->original_symbol);
        return res.get()->valueOrPanic();
    }

	/**
	 * @brief Deletes from mir Function (from block_order and blocks) unreachable blocks.
	 * Performs DFS on the CFG and marks every reachable block, then deletes the unreachable
	 * ones.
	 */
	Function eliminateUnreachable(Function function) {
		std::unordered_set<BlockID> reachable;
		std::stack<BlockID>         stack;

		stack.push(function.block_order[0]);
		while (!stack.empty()) {
			BlockID block_id = stack.top();
			stack.pop();

			if (reachable.contains(block_id)) continue;

			auto successors = getTerminatorSuccessors(function.blocks[block_id].terminator);

			reachable.insert(block_id);
			for (auto successor: successors) stack.push(successor);
		}

		std::vector<BlockID> new_block_order;

		for (auto block_id: function.block_order)
			if (reachable.contains(block_id))
				new_block_order.push_back(block_id);
			else
				function.blocks.erase(block_id);
		function.block_order = new_block_order;

		// WEAK_ASSERT candidate
		CORE_ASSERT(function.validateBlockIDs().isOk(), "Function has invalid block IDs");

		return function;
	}

	/**
	 * @brief Block with idx 0 of the MIR function has "FunctionEnd" terminator which is a
	 * mock-up.
	 *
	 * This function deals with this terminator:
	 * * if block doesn't exists it means that it was unreachable, we do nothing
	 * * if block is reachable, but function returns void it is replaced with ReturnVoid
	 * * if block is reachable and function returns value, throws missing return error
	 * @note It is assumed that the last block is the last in the block order.
	 */
	query::QResult<Function> finalizeFunctionEnd(query::Context& ctx, Function function) {
		CORE_ASSERT(
			function.blocks.size() > 0, "Function should have at least one block after lowering"
		);

		// It should be always zero because the last block is generated as the first one.
		auto last_block_id = BlockID(0);
		if (not function.blocks.contains(last_block_id)) return function;

		CORE_ASSERT(
			function.blocks[last_block_id].terminator.operation == Operation::FunctionEnd,
			"Last block doesn't have FunctionEnd terminator"
		);

		if (function.return_type.getType().getKind() == tsh::Kind::Unit) {
			function.blocks[last_block_id].terminator.operation = Operation::ReturnVoid;
			return function;
		} else {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"The function `",
					function.name,
					"` is missing a return statement or does not always return."
				),
				""
			));
			return query::Failed();
		}
	}

	struct IMPLEMENT_QUERY(LowerGlobalDataToMIRCtor, LowerGlobalDataToMIRFunctionResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			if (std::holds_alternative<helios::HOUTGlobalConst>(key.global_data->value))
				CORE_PANIC("Creating ctors for constant variables are not implemented yet.");

			auto global_init_expr
				= std::get<helios::HOUTGlobalVariable>(key.global_data->value).initial_value.ref();

			auto function_type = ctx.query<tsh::QueryFunctionType>({
				{},
				tsh::SymbolType{
					tsh::getUnitType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				},
			});

			// first step: lowering to pre-mir (cfg+quad)
			// create function builder
			FunctionBuilder function_builder{ ctx,
				                              GlobalVariableCTOR{ key.global_data->helios_symbol },
				                              function_type };
			function_builder.setName(base::StrID(
				base::strConcat("constructor_of_", key.global_data->original_name.strView()).c_str()
			));

			auto last_block = function_builder.newBlock();
			last_block->setTerminator(
				{ Operation::ReturnVoid, {}, {}, {}, function_builder.getTopLevelScope() }
			);

			auto assign_instr = last_block->addHole();

			auto lowerexpr_res = lowerExpr(
				*global_init_expr, last_block, function_builder, function_builder.getTopLevelScope()
			);

			assign_instr.fill(Instruction{
				Operation::Assign,
				{ MIRGlobal({ key.global_data->helios_symbol, key.global_data->type }) },
				{ lowerexpr_res.getResult(function_builder) },
				{},
				function_builder.getTopLevelScope(),
				{},
				{}
			});

			function_builder.setEntry(lowerexpr_res.begin);

			auto function_no_lifetime = function_builder.build();

			// second step: lifetime stuff
			auto function_with_destructors
				= runAllLifetimePasses(ctx, std::move(function_no_lifetime));

			// eliminating unreachable blocks
			auto function_reachable = eliminateUnreachable(std::move(function_with_destructors));

			if (validateFunction(ctx, function_reachable).isBad()) return query::Failed();

			return function_reachable;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerGlobalDataToMIRCtor)

	struct IMPLEMENT_QUERY(LowerToPreMIRFunction, LowerToMIRFunctionResult) {
		static Function build(Context& ctx, CRef<helios::HOUTFunction> function) {
            FunctionBuilder function_builder{
                ctx,
                FunctionSymID{ function->declaration->original_symbol },
            };
            function_builder.setName(function->declaration->original_name);

            LocalVarCollectionVisitor visitor{ function_builder };
            visitor.collect(function);

            auto last_block = function_builder.newBlock();
            last_block->setTerminator(
                { Operation::FunctionEnd, {}, {}, {}, function_builder.getTopLevelScope() }
            );

            auto first_block = lowerCodeBlock(
                *function->body, last_block, function_builder, function_builder.getTopLevelScope()
            );

            function_builder.setEntry(first_block.begin);

            return function_builder.build();
        }

        static auto provide(Context& ctx, const QKey& key) -> PResult {
            auto hout_func_q = ctx.query<helios::QueryCodeOfFun>(key);
            if (hout_func_q.get()->hasFailed()) return query::Failed();

            return build(ctx, base::CRef<helios::HOUTFunction>(&hout_func_q.get()->valueOrPanic()));
        }

		QUERY_AUTO_CACHE_CREF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToPreMIRFunction)

	struct IMPLEMENT_QUERY(LowerToMIRFunction, LowerToMIRFunctionResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// first step: lowering to pre-mir (cfg+quad)
			auto function_no_lifetime = ImplementationOf_LowerToPreMIRFunction::build(ctx, key.function);

			// second step: lifetime stuff
			auto function_with_destructors
				= runAllLifetimePasses(ctx, std::move(function_no_lifetime));

			// eliminating unreachable blocks
			auto function_reachable = eliminateUnreachable(std::move(function_with_destructors));

			// change FunctionEnd to proper return
			UNPACK_QRESULT_MOVE(
				auto function_no_func_end =, finalizeFunctionEnd(ctx, std::move(function_reachable))
			);

			if (validateFunction(ctx, function_no_func_end).isBad()) return query::Failed();

			return function_no_func_end;
		}

		QUERY_AUTO_CACHE_CREF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMIRFunction)

	ComptimeStatus isComptimeOnly(query::Context& ctx, helios::SymID sym_id) {
		auto decl_q = ctx.query<helios::QueryDeclOfFun>(sym_id);
		if (decl_q.get()->hasFailed()) return ComptimeStatus::Runtime;
		const auto& decl = decl_q.get()->valueOrPanic();

		auto pst_elem_opt = decl.origin.getPSTElement();
		if (!pst_elem_opt.has_value()) return ComptimeStatus::Runtime;

		auto root_element = pst_elem_opt.value();
		while (root_element.unlock(ctx)->getParent().has_value())
			root_element = root_element.unlock(ctx)->getParent().value();

		const auto& additional_root_data = root_element.unlock(ctx)->getAdditionalRootData();

		variant_match(additional_root_data.pst_parent) {
			variant_case(::pst::AdditionalRootData::ModuleParent, module_parent) {
				auto mod_id
					= base::anyCast<::compiler::frontend::ModuleID>(module_parent.module_id);

				auto module_comptime_map = ctx.query<ComptimeStatusCalculate>(mod_id);
				if (module_comptime_map.get()->hasFailed()) return ComptimeStatus::Runtime;

				const auto& actual_map = module_comptime_map.get()->valueOrPanic().map;
				if (actual_map.contains(sym_id)) return actual_map.at(sym_id);
			}
			variant_default {}
		}

		return ComptimeStatus::Runtime;
	}

	query::QResult<ModuleComptimeMap> comptimeStatusCalculate(
		query::Context& ctx, ::compiler::frontend::ModuleID mod_id
	) {
		ModuleComptimeMap result;

		std::unordered_map<helios::SymID, std::vector<helios::SymID>> reverse_call_graph;
		std::queue<helios::SymID>                                     infection_queue;

		auto hunit_q = ctx.query<helios::QueryModuleHOUT>(mod_id);
		if (hunit_q.get()->hasFailed()) return result;
		const auto& hunit = hunit_q.get()->valueOrPanic();

		for (const auto& hout_func: hunit.functions) {
			helios::SymID sym_id = hout_func->declaration->original_symbol;

			auto pre_mir_q = ctx.query<LowerToPreMIRFunction>(sym_id);
			if (pre_mir_q.get()->hasFailed()) continue;

			const Function& mir_func        = pre_mir_q.get()->valueOrPanic();
			bool            is_patient_zero = false;

			if (isComptimeOnlyType(mir_func.return_type.getType())) is_patient_zero = true;
			if (!is_patient_zero) {
				for (const auto& local: mir_func.local_list) {
					if (isComptimeOnlyType(local.type.getType())) {
						is_patient_zero = true;
						break;
					}
				}
			}
			if (!is_patient_zero) {
				for (auto block_id: mir_func.block_order) {
					if (!mir_func.blocks.contains(block_id)) continue;
					const auto& block = mir_func.blocks[block_id];

					for (const auto& instr: block.instructions) {
						if (isMetaOp(instr.operation)) {
							is_patient_zero = true;
							break;
						}
					}
					if (is_patient_zero) break;
				}
			}

			for (auto block_id: mir_func.block_order) {
				if (!mir_func.blocks.contains(block_id)) continue;
				const auto& block = mir_func.blocks[block_id];

				for (const auto& instr: block.instructions) {
					if (instr.operation == Operation::Call) {
						auto callee
							= instr.arguments.at(0).get<compiler::mir::MIRFunctionLiteral>();
						helios::SymID callee_id = callee.helios_id;
						reverse_call_graph[callee_id].push_back(sym_id);
					}
				}
			}

			if (is_patient_zero) {
				result.map[sym_id] = ComptimeStatus::ComptimeOnly;
				infection_queue.push(sym_id);
			}
		}

		while (!infection_queue.empty()) {
			auto current = infection_queue.front();
			infection_queue.pop();

			if (reverse_call_graph.contains(current)) {
				for (auto caller: reverse_call_graph[current]) {
					auto it = result.map.find(caller);
					if (it == result.map.end() || it->second != ComptimeStatus::ComptimeOnly) {
						result.map[caller] = ComptimeStatus::ComptimeOnly;
						infection_queue.push(caller);
					}
				}
			}
		}

		return result;
	}

	struct IMPLEMENT_QUERY(IsComptimeOnly, ComptimeStatusResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			return isComptimeOnly(ctx, key);
		}

		QUERY_AUTO_CACHE_CREF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(IsComptimeOnly)

	struct IMPLEMENT_QUERY(ComptimeStatusCalculate, ModuleComptimeMapResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			return comptimeStatusCalculate(ctx, key);
		}

		QUERY_AUTO_CACHE_CREF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(ComptimeStatusCalculate)
}
