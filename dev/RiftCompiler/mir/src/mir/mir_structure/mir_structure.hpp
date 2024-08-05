#pragma once

#include <vector>
#include <variant>
#include <helios/scopes/scopes.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_id.hpp>
#include <base/stringifyable_enum.hpp>

MAKE_STRINGIFYABLE_ENUM(
	compiler::mir,
	u64,
	Operation,
	Uninitialized,

	Call,
	VCall,
	IntegerAdd,  //< @TODO:some decisions here to be made about type stuff
                 // paraphs we want more generic code for MIR, so algos ar

	Destruct,
	DestructIf,

	VoidReturn,
	Return,
	Jump,
	Branch,

	FunctionEnd
);

namespace compiler::mir {

	/**
	 * @brief BlockID is a temporary solution that should be replaced by
	 * proper BlockReference.
	 * It is like that for now, to avoid confusion with BlockRef used in * mir_lowering and
	 * transformation between that BlockRef to this "BlockRef".
	 */
	STRONG_TYPEDEF_INT(BlockID, u64);

	struct MirIntegerConst final {
		i64 value;
	};

	STRONG_TYPEDEF_ID(LocalID);

	struct MirLocal final {
		// ...
		// @TODO: this needs some ids
		// @TODO: types
		LocalID id;

		MirLocal(): id(LocalID::next()) {}

		void debugPrint(std::ostream& output) const;
	};

	using LocalRef = base::StableVectorRef<MirLocal>;

	struct MirLocation final {
		// local / global / literal / func-literal, etc

	private:
		using ValueType = std::variant<MirIntegerConst, LocalRef, BlockID>;

		ValueType value;

	public:
		MirLocation(MirIntegerConst value): value(value) {}

		MirLocation(LocalRef value): value(value) {}

		MirLocation(BlockID value): value(value) {}

		void debugPrint(std::ostream& output) const;
	};

	struct OperationFlag final {};

	struct Instruction final {
		// Idea 1: generic arguments
		// Idea 2: one giant variant
		// Idea 3: inheritance

		Operation operation = Operation::Uninitialized;

		base::Optional<LocalRef> output;

		std::vector<MirLocation> arguments;

		// construct, destruct, move, ...:
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

	struct Block final {
		/**
		 * @brief id should be identical to the index in the vector of blocks.
		 */
		BlockID id;

		// add: scope info

		std::vector<Instruction> instructions;

		// It might be easier to have it in the vector and just add some asserts
		// this way some algorithms may be easier

		Instruction terminator;

		// void debugPrint(std::ostream& output) const;
	};

	struct Function final {
		base::StrId                  name;
		std::vector<Block>           blocks;
		base::StableVector<MirLocal> local_list;

		void debugPrint(std::ostream& output) const;
	};

}
