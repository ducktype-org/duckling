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
		/// Non-simple operations ///
		case Cast: {
			const auto cast_params = std::get_if<lir::CastParameters>(&instr.extra_params);
			CORE_ASSERT(cast_params != nullptr, "Cast instruction without parameters");
			return CastOperation{ *cast_params };
		}

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
			return SimpleOperation{ OpKind::cmpLt };
		case IntegerSLteq:
			return SimpleOperation{ OpKind::cmpLe };
		case IntegerSGt:
			return SimpleOperation{ OpKind::cmpGt };
		case IntegerSGteq:
			return SimpleOperation{ OpKind::cmpGe };

		/// Unsigned integer comparisons ///
		case IntegerULt:
			return SimpleOperation{ OpKind::ucmpLt };
		case IntegerULteq:
			return SimpleOperation{ OpKind::ucmpLe };
		case IntegerUGt:
			return SimpleOperation{ OpKind::ucmpGt };
		case IntegerUGteq:
			return SimpleOperation{ OpKind::ucmpGe };

		/// Floating point comparisons ///
		case FloatLt:
			return SimpleOperation{ OpKind::fcmpLt };
		case FloatGt:
			return SimpleOperation{ OpKind::fcmpGt };
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
		case AddressOf:
			return SimpleOperation{ OpKind::ref };
		case ZeroInitialize:
			return NoOpOperation{};
		case Call:
			return SimpleOperation{ OpKind::call };

		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
		CORE_UNREACHABLE();
	}


}
