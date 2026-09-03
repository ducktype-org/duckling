#pragma once

#include <helios/hout/elements/expr.hpp>
#include <mir/mir_structure/mir_lifetime_scope.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <vector>

namespace compiler::mir {
	struct BlockBuilder;
	struct FunctionBuilder;

	// @TODO: since BlockBuilderRef can be a parameter
	// we will need to add BlockBuilderRef->BlockRef transformation
	// during building phase
	using BlockBuilderRef = Ref<BlockBuilder>;

	/**
	 * @brief Structure representing block in build process.
	 * @note It is a builder in the sense of design pattern.
	 */
	struct BlockBuilder final {
	private:
		BlockID id;

		/**
		 * @brief List of instructions kept in revered order.
		 * If given position does not have a value that means it is empty.
		 */
		std::vector<base::Optional<Instruction>> reversed_instruction;
		base::Optional<Instruction>              terminator;

	public:
		/**
		 * @brief Structure representing a hole in the block, that is
		 * empty instruction that has to be filled, before the block will be builded.
		 */
		struct InstructionHole final {
		private:
			BlockBuilderRef block_ref;
			usize           position;

			[[nodiscard]]
			bool isEmpty() const;

			InstructionHole(BlockBuilderRef block_ref, usize position);

		public:
			void fill(Instruction instruction);

			void fillNop(ScopeRef scope);

			friend struct BlockBuilder;
		};

		BlockBuilder(usize vector_index);

		[[nodiscard]]
		Block build() const;

		/**
		 * @brief Adds instruction to the block.
		 * @note Instructions are added from last to first
		 * @param instr
		 */
		void addInstruction(Instruction instr);

		/**
		 * @brief Adds instruction hole, that can be filled later.
		 * @note It is needed when one does not know the instruction he has to add, before something
		 * else will be lowered.
		 * @return InstructionHole
		 */
		InstructionHole addHole();

		void setTerminator(Instruction instruction);

		[[nodiscard]]
		BlockID getID() const;
	};

	/**
	 * @brief Structure representing function in build process.
	 * @note It is a builder in the sense of design pattern.
	 */
	struct FunctionBuilder final {
	private:
		base::Optional<base::StrID>                                   name;
		base::StableVector<BlockBuilder>                              blocks;
		base::Optional<BlockBuilderRef>                               entry_block;
		base::StableVector<MIRLocal>                                  local_list;
		base::StableHashMap<helios::code::HOUTExprID, MIRLocalMutRef> reusable_expr_locals;
		tsh::FunctionAbstractType                                     function_type;

		/// Local id does not use global counter, since we want to keep the same id's
		/// in tests and in single run of the compiler (and in tests if global counter
		/// it would increment with each function compiler).
		u64 next_local_id{ 0 };

		LifetimeScopeTree lifetime_scope_tree;

		/**
		 * @brief Top level scope of the function.
		 * it is different from the root scope of litetime tree,
		 * since the root scope is the scope in which nothing
		 * should live.
		 * @important: This has to be defined below lifetime_scope_tree,
		 * since lifetime_scope_tree is used in its initialization.
		 */
		ScopeRef top_level_scope;

		/**
		 * @brief The scope that should be used for local variables
		 * that do not have a lifetime scope.
		 * @important: This has to be defined below lifetime_scope_tree,
		 * since lifetime_scope_tree is used in its initialization.
		 */
		ScopeRef no_lifetime_scope;

		query::Context& ctx;

		/**
		 * HELIOS SymID related to the function.
		 * Functions without a helios_id are functions created for eg. from expressions
		 */
		using HSymID = std::variant<FunctionSymID, GlobalVariableCtorDtor>;
		HSymID helios_symbol;

	public:
		FunctionBuilder(query::Context& ctx, const HSymID helios_symbol);

		FunctionBuilder(
			query::Context& ctx, const HSymID helios_symbol, tsh::FunctionAbstractType function_type
		);

		[[nodiscard]]
		Function build();

		void setName(base::StrID name);

		/**
		 * Adds a local variable to MIR function, from helios_id representing it.
		 */
		MIRLocalMutRef addLocal(helios::SymID helios_id);

		/**
		 * Adds a local parameter variable to MIR function from helios_id representing it.
		 */
		MIRLocalMutRef addParameter(helios::SymID helios_id, u64 parameter_index);

		/**
		 * Creates a temporary local value, identified by the reusable HOUT expression it
		 * represents. Also sets its lifetime scope.
		 * @note: If the temporary value for the given expression already exists, it will be
		 * returned instead of creating a new one.
		 */
		[[nodiscard]]
		MIRLocalMutRef getTmpForReusableExpr(
			const helios::code::ReusableExpr& reusable_expr, ScopeRef scope
		);

		/**
		 * Creates an anonymous temporary local value.
		 * Also sets its lifetime scope.
		 */
		[[nodiscard]]
		MIRLocalMutRef addTmp(tsh::SymbolType<> type, ScopeRef scope);

		/**
		 * Creates an anonymous temporary local value.
		 * It sets the proper special meaning to the local variable
		 * and proper local lifetime flags. The return value place
		 * should not have destructors nor scope flags.
		 */
		[[nodiscard]]
		MIRLocalMutRef addReturnTmp(tsh::SymbolType<> type);

		/**
		 * Add a temporary local value of type bool
		 * as a condition result temporary. It is used to store the result
		 * of conditions in ifs and loops, so that the condition value can
		 * be destructed properly and this local will not be destructed, as
		 * it is needed in the branching instruction.
		 */
		[[nodiscard]]
		MIRLocalMutRef addConditionTmp(ScopeRef scope);

		/**
		 * Finds the location of a local variable in the function. Does not check the global scope.
		 * @param helios_id The HELIoS symbol ID of the local variable.
		 * @return The local variable reference, if found.
		 */
		[[nodiscard]]
		base::Optional<MIRLocalMutRef> findLocal(const helios::SymID helios_id);

		[[nodiscard]]
		BlockBuilderRef newBlock();

		void setEntry(BlockBuilderRef block);

		[[nodiscard]]
		ScopeRef getTopLevelScope() const;

		[[nodiscard]]
		ScopeRef getNoLifetimeScope() const;

		[[nodiscard]]
		ScopeRef newScope(ScopeRef parent);

		[[nodiscard]]
		query::Context& getContext();

		/**
		 * This is needed only for some assertins.
		 */
		[[nodiscard]]
		HSymID getHeliosSymbol() const;
	};
}
