#include "mir_builders.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_lowering/mir_lifetimes.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::mir {
	[[nodiscard]]
	bool BlockBuilder::InstructionHole::isEmpty() const {
		return block_ref->reversed_instruction.at(position).empty();
	}

	BlockBuilder::InstructionHole::InstructionHole(BlockBuilderRef block_ref, usize position):
		  block_ref(block_ref),
		  position(position) {}

	void BlockBuilder::InstructionHole::fill(Instruction instruction) {
		CORE_ASSERT(isEmpty(), "Hole is already filled");
		CORE_ASSERT(
			not isTerminating(instruction.operation),
			"Instruction must not be a terminating instruction"
		);
		block_ref->reversed_instruction.at(position).emplace(std::move(instruction));
	}

	void BlockBuilder::InstructionHole::fillNop(ScopeRef scope) {
		fill(Instruction{
			Operation::Nop,
			{},
			{},
			{},
			scope,
		});
	}

	BlockBuilder::BlockBuilder(usize vector_index, const std::string_view debug_name):
		  id(vector_index) {
		if (not debug_name.empty()) this->debug_name.emplace(base::StrID(debug_name));
	}

	[[nodiscard]]
	Block BlockBuilder::build() const {
		std::vector<Instruction> instructions;
		for (const auto& instruction: reversed_instruction | std::views::reverse) {
			CORE_ASSERT(instruction.has_value(), "Empty instruction left in the block");
			instructions.emplace_back(instruction.value());
		}
		return {
			.id           = id,
			.instructions = std::move(instructions),
			.terminator   = terminator.value(),
			.debug_name   = debug_name,
		};
	}

	/**
	 * @brief Adds instruction to the block.
	 * @note Instructions are added from last to first
	 * @param instr
	 */
	void BlockBuilder::addInstruction(Instruction instr) {
		CORE_ASSERT(
			not isTerminating(instr.operation), "Instruction must not be a terminating instruction"
		);
		reversed_instruction.emplace_back(std::move(instr));
	}

	/**
	 * @brief Adds instruction hole, that can be filled later.
	 * @note It is needed when one does not know the instruction he has to add, before something
	 * else will be lowered.
	 * @return InstructionHole
	 */
	BlockBuilder::InstructionHole BlockBuilder::addHole() {
		// this emplaces empty optional:
		reversed_instruction.emplace_back();

		// creation of borrow pointer here, depends on the fact that blocks
		// are kept in stable container:
		return { this, reversed_instruction.size() - 1 };
	}

	void BlockBuilder::setTerminator(Instruction instruction) {
		CORE_ASSERT(not terminator.has_value(), "terminator already set.");
		CORE_ASSERT(
			isTerminating(instruction.operation), "Terminator must be a terminating instruction"
		);
		terminator.emplace(std::move(instruction));
	}

	[[nodiscard]]
	BlockID BlockBuilder::getID() const {
		return id;
	}

	FunctionBuilder::FunctionBuilder(query::Context& ctx, const HSymID helios_symbol):
		  function_type([&]() {
			  variant_match(helios_symbol) {
				  variant_case(FunctionSymID, fun_sym) {
					  return ctx.query<helios::QueryTypeOfSymbol>(fun_sym.id)
				          ->valueOrThrow()
				          .getType();
				  }
				  variant_default {
					  CORE_PANIC(
						  "FunctionBuilder constructor should be called only with FunctionSymID"
					  );
				  }
			  }

			  CORE_UNREACHABLE();
		  }()),
		  lifetime_scope_tree(),
		  top_level_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
		  no_lifetime_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
		  ctx(ctx),
		  helios_symbol(helios_symbol) {}

	FunctionBuilder::FunctionBuilder(
		query::Context& ctx, const HSymID helios_symbol, tsh::FunctionAbstractType function_type
	):
		  function_type(function_type),
		  top_level_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
		  no_lifetime_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
		  ctx(ctx),
		  helios_symbol(helios_symbol) {}

	[[nodiscard]]
	Function FunctionBuilder::build() {
		CORE_ASSERT(entry_block.has_value(), "Entry block not set");
		auto entry_block_id = entry_block.value()->getID();

		std::vector<BlockID> block_order;
		block_order.reserve(this->blocks.size());

		base::StableHashMap<BlockID, Block> function_blocks;

		// First element in block order is the entry block
		block_order.push_back(entry_block_id);

		// Count in "reverse order" to have more intuitive order
		// since creation of blocks is done from the end of the function.
		for (usize i = this->blocks.size(); i-- > 0;) {
			Block block = this->blocks[i]->build();
			function_blocks.put(block.id, std::move(block));

			if (block.id != entry_block_id)  // entry block is already added to the block_order
				block_order.emplace_back(block.id);
		}
		// we sanity check here, that all local variable have a lifetime scope,
		for (const auto& local: local_list)
			CORE_ASSERT(local.scope.has_value(), "Local variable without lifetime scope");

		return Function{
			name.value(),
			function_type.getResultType(),
			function_type.getParameterTypes(),
			std::move(function_blocks),
			std::move(block_order),
			std::move(local_list).toConstData(),
			next_local_id,
			std::move(lifetime_scope_tree),
			no_lifetime_scope,
			helios_symbol,
		};
	}

	void FunctionBuilder::setName(base::StrID name) {
		CORE_ASSERT(not this->name.has_value(), "Name already set");
		this->name.emplace(name);
	}

	/**
	 * Adds a local variable to MIR function, from helios_id representing it.
	 */
	MIRLocalMutRef FunctionBuilder::addLocal(const helios::SymID helios_id) {
		local_list.emplaceBack(MIRLocal{
			LocalID(next_local_id++),
			helios_id,
			ctx.query<helios::QueryTypeOfSymbol>(helios_id)->valueOrThrow(),
			{} });
		return local_list.last();
	}

	/**
	 * Adds a local parameter variable to MIR function from helios_id representing it.
	 */
	MIRLocalMutRef FunctionBuilder::addParameter(const helios::SymID helios_id, u64 parameter_index) {
		CORE_ASSERT(kind(helios_id) == helios::SymbolKind::Parameter, "Not a parameter");
		local_list.emplaceBack(MIRLocal{
			LocalID(next_local_id++),
			helios_id,
			ctx.query<helios::QueryTypeOfSymbol>(helios_id)->valueOrThrow(),
			LifetimeFlag::NoScopeFlags,
			parameter_index,
		});
		return local_list.last();
	}

	MIRLocalMutRef FunctionBuilder::getTmpForReusableExpr(
		const helios::code::ReusableExpr& reusable_expr, const ScopeRef scope
	) {
		auto expr_id = reusable_expr.inner->getID();
		if (auto found = reusable_expr_locals.atMaybeCopy(expr_id); found.has_value()) {
			auto tmp = found.value();
			// We have to widen the lifetime to cover all uses of the reusable expression.
			// When first_use and next_use live in different scopes (e.g. first_use before
			// a loop, next_use inside its body) the lifetime must span both, otherwise
			// the local gets pinned to the first scope we saw and writes from the other
			// use end up outside its lifetime window.
			tmp->scope.value() = lca(tmp->scope.value(), scope);
			return tmp;
		}

		const auto symbol_type = reusable_expr.expression_type.getSymbolType();
		auto       tmp         = addTmp(symbol_type, scope);
		reusable_expr_locals.put(expr_id, tmp);
		return tmp;
	}

	[[nodiscard]]
	MIRLocalMutRef FunctionBuilder::addTmp(const tsh::SymbolType<> type, const ScopeRef scope) {
		local_list.emplaceBack(MIRLocal{ LocalID(next_local_id++), type });
		auto tmp = local_list.last();
		tmp->setLifetimeScope(scope);
		return tmp;
	}

	/**
	 * Creates an anonymous temporary local value, i.e. local value
	 * not arising from variable written directly in the Duckling source code.
	 * Sets its lifetime scope to no_lifetime_scope.
	 */
	[[nodiscard]]
	MIRLocalMutRef FunctionBuilder::addReturnTmp(const tsh::SymbolType<> type) {
		auto tmp = addTmp(type, no_lifetime_scope);
		tmp->lifetime_flags |= LifetimeFlag::ReturnTmpValue | LifetimeFlag::NoScopeFlags
		                     | LifetimeFlag::NoDestructor;
		return tmp;
	}

	/**
	 * Add a temporary local value of type bool.
	 * Sets its lifetime scope to no_lifetime_scope.
	 * Used for example by if/while lowering to store
	 * the result of the condition.
	 */
	[[nodiscard]]
	MIRLocalMutRef FunctionBuilder::addConditionTmp(ScopeRef scope) {
		auto type = tsh::SymbolType<>(
			tsh::getBoolType(), tsh::ReferenceKind::Direct, tsh::Mutability::Immutable
		);
		auto tmp = addTmp(type, scope);
		tmp->lifetime_flags |= LifetimeFlag::ConditionTmpValue | LifetimeFlag::NoDestructor;
		return tmp;
	}

	/**
	 * Finds the location of a local variable in the function. Does not check the global scope.
	 * @param helios_id The HELIoS symbol ID of the local variable.
	 * @return The local variable reference, if found.
	 */
	[[nodiscard]]
	base::Optional<MIRLocalMutRef> FunctionBuilder::findLocal(const helios::SymID helios_id) {
		// @TODO: Optimize into a hashmap.
		for (auto& local: local_list)
			if (local.helios_id == helios_id) return &local;
		return {};
	}

	[[nodiscard]]
	BlockBuilderRef FunctionBuilder::newBlock(const std::string_view debug_name) {
		auto vector_index = blocks.size();
		blocks.emplaceBack(BlockBuilder{ vector_index, debug_name });
		CORE_ASSERT(u64(blocks.last()->getID()) == blocks.lastIndex(), "Bad block id");
		return blocks.last();
	}

	void FunctionBuilder::setEntry(BlockBuilderRef block) {
		CORE_ASSERT(entry_block.empty(), "Entry block already set.");
		entry_block.emplace(block);
	}

	[[nodiscard]]
	ScopeRef FunctionBuilder::getTopLevelScope() const {
		return top_level_scope;
	}

	[[nodiscard]]
	ScopeRef FunctionBuilder::getNoLifetimeScope() const {
		return no_lifetime_scope;
	}

	[[nodiscard]]
	ScopeRef FunctionBuilder::newScope(ScopeRef parent) {
		return lifetime_scope_tree.newScope(parent);
	}

	[[nodiscard]]
	query::Context& FunctionBuilder::getContext() {
		return ctx;
	}

	/**
	 * This is needed only for some assertins.
	 */
	[[nodiscard]]
	FunctionBuilder::HSymID FunctionBuilder::getHeliosSymbol() const {
		return helios_symbol;
	}
}
