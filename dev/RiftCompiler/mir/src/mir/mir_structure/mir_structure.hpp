#pragma once

#include <vector>
#include <variant>
#include <helios/scopes/scopes.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_id.hpp>
#include <base/stringifyable_enum.hpp>

// clang-format off
MAKE_STRINGIFYABLE_ENUM(compiler::mir, u64, Operation,
	Uninitialized,

	Call,
	VCall,

	/**
	 * @brief Placeholder. 
	 * @todo  Some decisions here to be made about operations like that.
     * Perhaps we want more generic code for MIR, so algorithms are simpler.
	 * There could be single operation for all Add, Sub, etc, and single one for all
	 * comparisons. 
	 */
	IntegerAdd,

	Destruct,
	DestructIf,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch,

	/**
	 * @brief Operation that represents end of a function.
	 * @note  It is always implicitly added at the end of a function.
	 * This operation can have different meaning depending on the context.
	 * For example in a function that returns void, it is just a return.
	 * In a function that returns value, "it is" an compiler error, unless its
	 * unreachable.
	 */
	FunctionEnd
);

// clang-format on

namespace compiler::mir {

	/**
	 * @brief Whether given operation is an operation that can (ans has to be)
	 * the last operation in the block (i.e. be a terminator).
	 */
	bool isTerminating(Operation);

	/**
	 * @brief BlockID is a temporary solution that should be replaced by
	 * proper BlockReference.
	 * It is like that for now, to avoid confusion with BlockRef used in mir_lowering and
	 * transformation between that BlockRef to this "BlockRef".
	 */
	STRONG_TYPEDEF_INT(BlockID, u64);

	struct MirIntegerConst final {
		i64 value;
	};

	STRONG_TYPEDEF_ID(LocalID);

	struct MirLocal final {
		LocalID id;

		// @TODO: type

		MirLocal(): id(LocalID::next()) {}

		void debugPrint(std::ostream& output) const;
	};

	using LocalRef = base::StableVectorRef<MirLocal>;

	struct MirLocation final {
	private:
		// @TODO: global, literal, func-literal, ...
		using ValueType = std::variant<MirIntegerConst, LocalRef, BlockID>;

		ValueType value;

	public:
		MirLocation(MirIntegerConst value): value(value) {}

		MirLocation(LocalRef value): value(value) {}

		MirLocation(BlockID value): value(value) {}

		void debugPrint(std::ostream& output) const;
	};

	/**
	 * @brief Structure representing meta informations about operation
	 * such as:
	 * * does operation construct some variable
	 * * does operation destruct some variable
	 * * does operation move some variable
	 */
	struct OperationFlag final {};

	/**
	 * @brief Single instruction of MIR code.
	 *
	 */
	struct Instruction final {
		Operation operation = Operation::Uninitialized;

		base::Optional<LocalRef> output;

		std::vector<MirLocation> arguments;

		// construct, destruct, move.
		std::vector<OperationFlag> flags;

		// @TODO: each Instruction should have source position reference

		//  perhaps just map during MIR creation? (mir scope -- just super simple tree)
		helios::ScopeID scope;

		Instruction()                   = delete;
		Instruction(const Instruction&) = default;
		Instruction(Instruction&&)      = default;

		Instruction(
			Operation                  operation,
			base::Optional<LocalRef>   output,
			std::vector<MirLocation>   arguments,
			std::vector<OperationFlag> flags,
			helios::ScopeID            scope
		):
			  operation(operation),
			  output(std::move(output)),
			  arguments(std::move(arguments)),
			  flags(std::move(flags)),
			  scope(std::move(scope)) {}

		void debugPrint(std::ostream& output) const;
	};

	/**
	 * @brief A simple block of MIR cfg code.
	 */
	struct Block final {
		/**
		 * @brief Unique identifier of the block.
		 * @note it must be identical to the index in the vector of blocks.
		 */
		BlockID id;

		std::vector<Instruction> instructions;

		/**
		 * @brief Last instruction of the block.
		 * It has to be terminating instruction (branch, return, etc).
		 *
		 * @todo: Decide if we wan't to move it to instruction vector.
		 */
		Instruction terminator;
	};

	/**
	 * @brief Function in MIR.
	 */
	struct Function final {
		base::StrId                  name;
		std::vector<Block>           blocks;
		base::StableVector<MirLocal> local_list;
		BlockID                      entry_block;

		void debugPrint(std::ostream& output) const;
	};

}
