#pragma once

#include <vector>
#include <helios/scopes/scopes.hpp>
#include <base/stable_container.hpp>

namespace compiler::mir {	
	struct MirIntegerConst {
		i32 value;
	};

	struct MirLocal {
		// ...
		// @TODO: this needs some ids
	};
	using LocalRef = base::StableVectorRef<MirLocal>;


	struct MirLocation {
		// local / global / literal / func-literal, etc
	private:
		std::variant<MirIntegerConst, LocalRef> value;
	public:

	};

	// @TODO: OperationKind === Operation?

	enum class OperationKind {
		// This are just object markings....
		Construct, Temporary, Move, Reassign, Destruct
	};

	struct OperationFlag {

	};

	enum class Operation {
		Call,
		VCall,
		IntegerAdd, //< @TODO:some decisions here to be made about type stuff
					// paraphs we want more generic code for MIR, so algos ar 

		Destruct, // @TODO: is this operation? Paraph it should just be call to destructor with special destruct marking...
	};

	struct Instruction {
		// Idea 1: generic arguments
		// Idea 2: one giant variant
		// Idea 3: inheritance

		Operation operation;

		std::vector<MirLocation> arguments;

		// @TODO: each Instruction should have source position reference

		// Optional<> return
		// Operation oper
		// vector<MirValue> arguments
	};

	struct Terminator {
		// Is it separate?

		// Jump / Branch / Return / ...
	};

	struct Block {
		// add: scope info

		std::vector<Instruction> instructions;

		// It might be easier to have it in the vector and just add some asserts
		// this way some algorithms may be easier
		
		Instruction terminator;

		//  perhaps just map during MIR creation? (mir scope -- just super simple tree)
		helios::ScopeID scope;
	};

	struct Function {
		std::vector<Block> blocks;
		base::StableVector<MirLocal> local_list;
	};

}
