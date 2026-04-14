#include "dvm_operation.hpp"

#include <lir/lir_structure/lir_structure.hpp>

namespace {
	using namespace compiler;

	bool isMetaTypeOperation(lir::Operation op) {
		return op == lir::Operation::MetaCreateBox || op == lir::Operation::MetaCreateRef
		    || op == lir::Operation::MetaCreateConst || op == lir::Operation::MetaCreateTuple
		    || op == lir::Operation::MetaCreateVariant || op == lir::Operation::MetaEq
		    || op == lir::Operation::MetaNeq;
	}
}

namespace compiler::backend_vm::internal {

	DVMOperation lirInstrToDVMOperation(const lir::Instruction& instr) {
		auto operation = instr.operation;
		if (isMetaTypeOperation(operation)) return MetaOperation{ operation };

		using enum lir::Operation;

		switch (operation) {
		/// Special operations ///
		case Cast: {
			const auto cast_params = std::get_if<lir::CastParameters>(&instr.extra_params);
			CORE_ASSERT(cast_params != nullptr, "Cast instruction without parameters");
			return CastOperation{ *cast_params };
		}
		case Call:
			return CallOperation{};
		case AddressOf:
			return AddressOfOperation{};
		case ZeroInitialize:
			// Data in DVM is zeroinitialized by default, so this is a NoOp.
			return NoOpOperation{};
		case Assign:  // TODOP: Special operation?
			return MoveOperation{};

		/// Unary operations ///
		case IntegerNeg:
			return UnaryOperation{ OpKind::neg };
		case FloatNeg:
			return UnaryOperation{ OpKind::fneg };
		case BooleanNot:
			return UnaryOperation{ OpKind::log_not };

		/// Binary operations ///
		case IntegerAdd:
			return BinaryOperation{ OpKind::add };
		case IntegerSub:
			return BinaryOperation{ OpKind::sub };
		case IntegerMul:
			return BinaryOperation{ OpKind::mul };
		case IntegerSDiv:
			return BinaryOperation{ OpKind::div };
		case IntegerSMod:
			return BinaryOperation{ OpKind::mod };
		case IntegerUDiv:
			return BinaryOperation{ OpKind::udiv };
		case IntegerUMod:
			return BinaryOperation{ OpKind::umod };

		case FloatAdd:
			return BinaryOperation{ OpKind::fadd };
		case FloatSub:
			return BinaryOperation{ OpKind::fsub };
		case FloatMul:
			return BinaryOperation{ OpKind::fmul };
		case FloatDiv:
			return BinaryOperation{ OpKind::fdiv };

		case BooleanAnd:
			return BinaryOperation{ OpKind::log_and };
		case BooleanOr:
			return BinaryOperation{ OpKind::log_or };

		/// Comparison operations ///
		case IntegerEq:
			return ComparisonOperation{ OpKind::cmpEq };
		case IntegerNeq:
			return ComparisonOperation{ OpKind::cmpNeq };
		case IntegerSLt:
			return ComparisonOperation{ OpKind::cmpLt };
		case IntegerSLteq:
			return ComparisonOperation{ OpKind::cmpLe };
		case IntegerSGt:
			return ComparisonOperation{ OpKind::cmpGt };
		case IntegerSGteq:
			return ComparisonOperation{ OpKind::cmpGe };

		case IntegerULt:
			return ComparisonOperation{ OpKind::ucmpLt };
		case IntegerULteq:
			return ComparisonOperation{ OpKind::ucmpLe };
		case IntegerUGt:
			return ComparisonOperation{ OpKind::ucmpGt };
		case IntegerUGteq:
			return ComparisonOperation{ OpKind::ucmpGe };

		case FloatLt:
			return ComparisonOperation{ OpKind::fcmpLt };
		case FloatGt:
			return ComparisonOperation{ OpKind::fcmpGt };
		case FloatLteq:
			return ComparisonOperation{ OpKind::fcmpLe };
		case FloatGteq:
			return ComparisonOperation{ OpKind::fcmpGe };
		case FloatEq:
			return ComparisonOperation{ OpKind::fcmpEq };
		case FloatNeq:
			return ComparisonOperation{ OpKind::fcmpNeq };

		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
	}
}
