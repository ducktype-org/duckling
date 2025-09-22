#include "mir_querries.hpp"

#include "../mir_structure/mir_structure.hpp"

#include <helios/helios_errors.hpp>
#include <helios/hout/hout.hpp>
#include <mir_private/mir_lowering.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::mir {
	u64 KeyOf_LowerToMirFunction::queryUnstablePerfectHash() const {
		return function.queryUnstablePerfectHash();
	}

	u64 KeyOf_LowerGlobalDataToMirFunction::queryUnstablePerfectHash() const {
		return global_data.helios_symbol.queryUnstablePerfectHash();
	}

	Function lowerToPreMirFunction(query::Context& ctx, const helios::HOUTFunction& function) {
		FunctionBuilder function_builder{ ctx, FunctionSymID{ function.original_symbol } };
		function_builder.setName(function.original_name);

		LocalVarCollectionVisitor visitor{ function_builder };
		visitor.collect(function);

		auto last_block = function_builder.newBlock();
		last_block->setTerminator(
			{ Operation::FunctionEnd, {}, {}, {}, function_builder.getTopLevelScope() }
		);

		// build cfg+quad step by step:
		auto first_block = lowerCodeBlock(
			*function.content.body, last_block, function_builder, function_builder.getTopLevelScope()
		);

		function_builder.setEntry(first_block.begin);

		return function_builder.build();
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
	query::QResult<Function, helios::errors::Failed> finalizeFunctionEnd(
		query::Context&, Function function
	) {
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
			// @todo there should be logging here of missing return value / control reaches the
			// end of non-void function
			return query::QError(helios::errors::Failed());
		}
	}

	struct IMPLEMENT_QUERY(LowerGlobalDataToMirCtor, LowerGlobalDataToMirFunctionResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			if (std::holds_alternative<helios::HOUTGlobalConst>(key.global_data.value))
				CORE_PANIC("Creating ctors for constant variables are not implemented yet.");

			auto global_init_expr
				= std::get<helios::HOUTGlobalVariable>(key.global_data.value).initial_value->ref();

			auto function_type = ctx.query<tsh::QueryFunctionType>({
				{},
				tsh::SymbolType{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				},
			});

			// first step: lowering to pre-mir (cfg+quad)
			// create function builder
			FunctionBuilder function_builder{ ctx,
				                              GlobalVariableCTOR{ key.global_data.helios_symbol },
				                              function_type };
			function_builder.setName(
				base::StrID(base::strConcat(
								"_GLOBAL_",
								key.global_data.original_name,
								key.global_data.helios_symbol.queryUnstablePerfectHash()
				)
			                    .c_str())
			);

			auto last_block = function_builder.newBlock();
			last_block->setTerminator(
				{ Operation::ReturnVoid, {}, {}, {}, function_builder.getTopLevelScope() }
			);

			auto assing_instr = last_block->addHole();

			auto lowerexpr_res = lowerExpr(
				*global_init_expr.get(),
				last_block,
				function_builder,
				function_builder.getTopLevelScope()
			);

			assing_instr.fill(Instruction{
				Operation::Assign,
				{ MirGlobal({ key.global_data.helios_symbol, key.global_data.type }) },
				{ lowerexpr_res.getResult(function_builder) },
				{},
				function_builder.getTopLevelScope(),
			});

			function_builder.setEntry(lowerexpr_res.begin);

			auto function_no_lifetime = function_builder.build();

			// second step: lifetime stuff
			auto function_with_destructors = addDestructors(ctx, std::move(function_no_lifetime));

			// eliminating unreachable blocks
			auto function_reachable = eliminateUnreachable(std::move(function_with_destructors));

			if (validateFunction(function_reachable).isBad())
				return query::QError(helios::errors::Failed());

			return function_reachable;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerGlobalDataToMirCtor)

	struct IMPLEMENT_QUERY(LowerToMirFunction, LowerToMirFunctionResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// first step: lowering to pre-mir (cfg+quad)
			auto function_no_lifetime = lowerToPreMirFunction(ctx, key.function);

			// second step: lifetime stuff
			auto function_with_destructors = addDestructors(ctx, std::move(function_no_lifetime));

			// eliminating unreachable blocks
			auto function_reachable = eliminateUnreachable(std::move(function_with_destructors));

			// change FunctionEnd to proper return
			UNPACK_RESULT_MOVE(
				auto function_no_func_end =, finalizeFunctionEnd(ctx, std::move(function_reachable))
			);

			return function_no_func_end;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);
}
