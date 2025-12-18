#include "dvm_operation.hpp"

namespace {
	using namespace compiler;

	bool isMetaTypeOperation(lir::Operation op) {
		return op == lir::Operation::MetaCreateBox || op == lir::Operation::MetaCreateRef
		    || op == lir::Operation::MetaCreateTuple || op == lir::Operation::MetaCreateVariant
		    || op == lir::Operation::MetaEq || op == lir::Operation::MetaNeq;
	}
}

namespace compiler::backend_vm::internal {
	DVMOperation lirOpToDVMOperation(lir::Operation operation) {
		if (isMetaTypeOperation(operation)) return MetaOperation{ operation };

		using enum lir::Operation;

		switch (operation) {
		/// Integer operations ///
		case IntegerAdd:
			return SimpleOperation{ OpKind::add };
		case IntegerSub:
			return SimpleOperation{ OpKind::sub };
		case IntegerNeg:
			return SimpleOperation{ OpKind::neg };
		case IntegerMul:
			return SimpleOperation{ OpKind::mul };
		case IntegerSDiv:
			return SimpleOperation{ OpKind::div };
		case IntegerSMod:
			return SimpleOperation{ OpKind::mod };
		case IntegerUDiv:
			return SimpleOperation{ OpKind::udiv };
		case IntegerUMod:
			return SimpleOperation{ OpKind::umod };

		/// Floating point operations ///
		case FloatAdd:
			return SimpleOperation{ OpKind::fadd };
		case FloatSub:
			return SimpleOperation{ OpKind::fsub };
		case FloatMul:
			return SimpleOperation{ OpKind::fmul };
		case FloatDiv:
			return SimpleOperation{ OpKind::fdiv };
		case FloatNeg:
			return SimpleOperation{ OpKind::fneg };

		/// Signed integer comparisons ///
		case IntegerEq:
			return SimpleOperation{ OpKind::cmpEq };
		case IntegerNeq:
			return SimpleOperation{ OpKind::cmpNeq };
		case IntegerSLt:
			return SimpleOperation{ OpKind::cmpL };
		case IntegerSLteq:
			return SimpleOperation{ OpKind::cmpLe };
		case IntegerSGt:
			return SimpleOperation{ OpKind::cmpG };
		case IntegerSGteq:
			return SimpleOperation{ OpKind::cmpGe };

		/// Unsigned integer comparisons ///
		case IntegerULt:
			return SimpleOperation{ OpKind::ucmpL };
		case IntegerULteq:
			return SimpleOperation{ OpKind::ucmpLe };
		case IntegerUGt:
			return SimpleOperation{ OpKind::ucmpG };
		case IntegerUGteq:
			return SimpleOperation{ OpKind::ucmpGe };

		/// Floating point comparisons ///
		case FloatLt:
			return SimpleOperation{ OpKind::fcmpL };
		case FloatGt:
			return SimpleOperation{ OpKind::fcmpG };
		case FloatLteq:
			return SimpleOperation{ OpKind::fcmpLe };
		case FloatGteq:
			return SimpleOperation{ OpKind::fcmpGe };
		case FloatEq:
			return SimpleOperation{ OpKind::fcmpEq };
		case FloatNeq:
			return SimpleOperation{ OpKind::fcmpNeq };

		/// Logical operations ///
		case BooleanAnd:
			return SimpleOperation{ OpKind::log_and };
		case BooleanOr:
			return SimpleOperation{ OpKind::log_or };
		case BooleanNot:
			return SimpleOperation{ OpKind::log_not };

		/// Other ///
		case Assign:
			return SimpleOperation{ OpKind::mov };
		case Call:
			return SimpleOperation{ OpKind::call };

		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
		CORE_UNREACHABLE();
	}


}
