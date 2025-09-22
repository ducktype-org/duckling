#pragma once

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/query_impl.hpp>

#include <base/variant.hpp>

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
			bool isEmpty() const {
				return block_ref->reversed_instruction.at(position).empty();
			}

			InstructionHole(BlockBuilderRef block_ref, usize position):
				  block_ref(block_ref),
				  position(position) {}

		public:
			void fill(Instruction instruction) {
				CORE_ASSERT(isEmpty(), "Hole is already filled");
				CORE_ASSERT(
					not isTerminating(instruction.operation),
					"Instruction must not be a terminating instruction"
				);
				block_ref->reversed_instruction.at(position).emplace(std::move(instruction));
			}

			void fillNop(ScopeRef scope) {
				fill(Instruction{
					Operation::Nop,
					{},
					{},
					{},
					scope,
				});
			}

			friend struct BlockBuilder;
		};

		BlockBuilder(usize vector_index): id(vector_index) {}

		[[nodiscard]]
		Block build() const {
			std::vector<Instruction> instructions;
			for (const auto& instruction: reversed_instruction | std::views::reverse) {
				CORE_ASSERT(instruction.has_value(), "Empty instruction left in the block");
				instructions.emplace_back(instruction.value());
			}
			return {
				.id           = id,
				.instructions = std::move(instructions),
				.terminator   = terminator.value(),
			};
		}

		/**
		 * @brief Adds instruction to the block.
		 * @note Instructions are added from last to first
		 * @param instr
		 */
		void addInstruction(Instruction instr) {
			CORE_ASSERT(
				not isTerminating(instr.operation),
				"Instruction must not be a terminating instruction"
			);
			reversed_instruction.emplace_back(std::move(instr));
		}

		/**
		 * @brief Adds instruction hole, that can be filled later.
		 * @note It is needed when one does not know the instruction he has to add, before something
		 * else will be lowered.
		 * @return InstructionHole
		 */
		InstructionHole addHole() {
			// this emplaces empty optional:
			reversed_instruction.emplace_back();

			// creation of borrow pointer here, depends on the fact that blocks
			// are kept in stable container:
			return { this, reversed_instruction.size() - 1 };
		}

		void setTerminator(Instruction instruction) {
			CORE_ASSERT(not terminator.has_value(), "terminator already set.");
			CORE_ASSERT(
				isTerminating(instruction.operation), "Terminator must be a terminating instruction"
			);
			terminator.emplace(std::move(instruction));
		}

		[[nodiscard]]
		BlockID getID() const {
			return id;
		}
	};

	/**
	 * @brief Structure representing function in build process.
	 * @note It is a builder in the sense of design pattern.
	 */
	struct FunctionBuilder final {
	private:
		base::Optional<base::StrID>      name;
		base::StableVector<BlockBuilder> blocks;
		base::Optional<BlockBuilderRef>  entry_block;
		base::StableVector<MirLocal>     local_list;
		tsh::FunctionAbstractType        function_type;

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
		 * HELIOS SymID releted to the function.
		 * Functions without a helios_id are functions created for eg. from expressions
		 */
		using HSymID = std::variant<FunctionSymID, GlobalVariableCTOR>;
		HSymID helios_symbol;

	public:
		FunctionBuilder(query::Context& ctx, const HSymID helios_symbol):
			  function_type([&]() {
				  variant_match(helios_symbol) {
					  variant_case(FunctionSymID, fun_sym) {
						  return ctx.query<helios::QueryTypeOfSymbol>(fun_sym.id)
					          ->expect("Handling errors in MIR is not supported yet")
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

		FunctionBuilder(
			query::Context& ctx, const HSymID helios_symbol, tsh::FunctionAbstractType function_type
		):
			  function_type(function_type),
			  top_level_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
			  no_lifetime_scope(lifetime_scope_tree.newScope(lifetime_scope_tree.root)),
			  ctx(ctx),
			  helios_symbol(helios_symbol) {}

		[[nodiscard]]
		Function build() {
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
				std::move(lifetime_scope_tree),
				no_lifetime_scope,
				helios_symbol,
			};
		}

		void setName(base::StrID name) {
			CORE_ASSERT(not this->name.has_value(), "Name already set");
			this->name.emplace(name);
		}

		/**
		 * Adds a local variable to MIR function, from helios_id representing it.
		 */
		MutLocalRef addLocal(const helios::SymID helios_id) {
			local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
			});
			return local_list.last();
		}

		/**
		 * Adds a local parameter variable to MIR function from helios_id representing it.
		 */
		MutLocalRef addParameter(const helios::SymID helios_id, u64 parameter_index) {
			CORE_ASSERT(kind(helios_id) == helios::SymbolKind::Parameter, "Not a parameter");
			local_list.emplaceBack(MirLocal{
				helios_id,
				ctx.query<helios::QueryTypeOfSymbol>(helios_id)->expect(
					"Handling ERRORS in MIR is not supported yet..."
				),
				parameter_index,
			});
			return local_list.last();
		}

		/**
		 * Creates a temporary local value, and also sets its lifetime scope.
		 */
		[[nodiscard]]
		MutLocalRef addTmp(const tsh::SymbolType<> type, ScopeRef scope) {
			local_list.emplaceBack(MirLocal{ type });
			auto tmp = local_list.last();
			tmp->setLifetimeScope(scope);
			return tmp;
		}

		/**
		 * Creates a temporary local value, i.e. local value
		 * not arising from variable written directly in the Duckling source code.
		 * Sets its lifetime scope to no_lifetime_scope.
		 */
		[[nodiscard]]
		MutLocalRef addNoLifetimeTmp(const tsh::SymbolType<> type) {
			return addTmp(type, no_lifetime_scope);
		}

		/**
		 * Add a temporary local value of type bool.
		 * Sets its lifetime scope to no_lifetime_scope.
		 * Used for example by if/while lowering to store
		 * the result of the condition.
		 */
		[[nodiscard]]
		MutLocalRef addNoLifetimeBoolTmp() {
			auto type = tsh::SymbolType<>(
				ctx.query<tsh::QueryBoolType>({}),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable
			);
			return addNoLifetimeTmp(type);
		}

		/**
		 * Finds the location of a local variable in the function. Does not check the global scope.
		 * @param helios_id The HELIoS symbol ID of the local variable.
		 * @return The local variable reference, if found.
		 */
		[[nodiscard]]
		base::Optional<MutLocalRef> findLocal(const helios::SymID helios_id) {
			// @TODO: Optimize into a hashmap.
			for (auto& local: local_list)
				if (local.helios_id == helios_id) return &local;
			return {};
		}

		[[nodiscard]]
		BlockBuilderRef newBlock() {
			auto vector_index = blocks.size();
			blocks.emplaceBack(BlockBuilder{ vector_index });
			CORE_ASSERT(u64(blocks.last()->getID()) == blocks.lastIndex(), "Bad block id");
			return blocks.last();
		}

		void setEntry(BlockBuilderRef block) {
			CORE_ASSERT(entry_block.empty(), "Entry block already set.");
			entry_block.emplace(block);
		}

		[[nodiscard]]
		auto getTopLevelScope() const {
			return top_level_scope;
		}

		[[nodiscard]]
		auto getNoLifetimeScope() const {
			return no_lifetime_scope;
		}

		[[nodiscard]]
		auto newScope(ScopeRef parent) {
			return lifetime_scope_tree.newScope(parent);
		}

		[[nodiscard]]
		query::Context& getContext() {
			return ctx;
		}

		/**
		 * This is needed only for some assertins.
		 */
		[[nodiscard]]
		HSymID getHeliosSymbol() const {
			return helios_symbol;
		}
	};
}