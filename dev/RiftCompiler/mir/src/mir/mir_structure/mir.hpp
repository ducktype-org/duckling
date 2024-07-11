#pragma once

#include <vector>

namespace compiler::mir {
	
	struct MirValue {
		// local / global / literal / func-literal
	};

	// @TODO: OperationKind === Operation?

	enum OperationKind {
		Construct, Temporary, Move, Reassign, Destruct
	};

	enum Operation {

	};

	struct Instruction {
		// Optional<> return
		// Operation oper
		// vector<MirValue> arguments
	};

	struct Terminator {
		// Jump / Branch / Return / ...
	};

	struct Block {
		std::vector<Instruction> instructions;
		Instruction terminator;
		// ScopeID scope ? -- perhaps just map during MIR creation
	};

	struct Function {
		std::vector<Block> blocks;
	};

}
