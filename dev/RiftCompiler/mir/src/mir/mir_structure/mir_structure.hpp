#pragma once

#include <vector>

namespace compiler::mir {
	
	struct MirValue {
		// local / global / literal / func-literal
	};

	// @TODO: OperationKind === Operation?

	enum class OperationKind {
		// This are just object markings....
		Construct, Temporary, Move, Reassign, Destruct
	};

	enum class Operation {
		Call,
		VCall,
		IntegerAdd, //< @TODO:some decisions here to be made about type stuff
					// paraphs we want more generic code for MIR, so algos ar 

		Destruct, // @TODO: is this operation? Paraph it should just be call to destructor with special destruct marking...
	};


	struct Argument {
		// Imm or local/tmp or global or imm func, 
		// perhaps a wrapper to variant?
		// types..
	};

	struct Instruction {
		// Idea 1: generic arguments
		// Idea 2: one giant variant
		// Idea 3: inheritance

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
		// ScopeID scope ? -- perhaps just map during MIR creation
	};

	struct Function {
		std::vector<Block> blocks;
	};

}
