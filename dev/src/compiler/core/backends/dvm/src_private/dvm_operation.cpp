#include "dvm_operation.hpp"

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
	/**
	 * @brief Picks the right DVM operation for a Cast LIR instruction to.
	 */
	DVMOperation lirCastParamsToDVMOperation(const lir::CastParameters& cast_params) {
		const auto  target_layout = cast_params.target_layout;
		const auto  source_layout = cast_params.source_layout;
		const auto& source_type   = cast_params.source_type;
		const auto& target_type   = cast_params.target_type;

		auto is_signed = [](const tsh::SymbolType<>& type) -> bool {
			if (type.getType().getKind() == tsh::Kind::Integral) {
				return (
					tsh::IntegralAbstractType(type.getType()).getSignedness()
					== tsh::IntegralAbstractType::Signedness::Signed
				);
			}
			// Char, bool, etc. are treated as unsigned
			return false;
		};

		variant_match(source_layout->getVariant()) {
			variant_case_novalue(tsl::IntegralTypeLayout) {
				const bool src_signed = is_signed(source_type);
				variant_match(target_layout->getVariant()) {
					variant_case_novalue(tsl::IntegralTypeLayout) {
						// ================== Int -> Int ==================
						if (source_layout->getSize() < target_layout->getSize()) {
							// Extend
							return SimpleOperation{ src_signed ? OpKind::sext : OpKind::zext };
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return SimpleOperation{ OpKind::trunc };
						} else {
							// No-op cast
							return SimpleOperation{ OpKind::mov };
						}
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Int -> Float ==================
						return SimpleOperation{ src_signed ? OpKind::sitofp : OpKind::uitofp };
					}
					variant_default {
						CORE_PANIC("Unsupported cast from integral-layout to target layout");
					}
				}
			}
			variant_case_novalue(tsl::FloatTypeLayout) {
				variant_match(target_layout->getVariant()) {
					variant_case_novalue(tsl::IntegralTypeLayout) {
						// ================== Float -> Int ==================
						const bool to_signed = is_signed(target_type);
						return SimpleOperation{ to_signed ? OpKind::fptosi : OpKind::fptoui };
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Float -> Float ==================
						if (source_layout->getSize() < target_layout->getSize()) {
							// Extend
							return SimpleOperation{ OpKind::fpext };
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return SimpleOperation{ OpKind::fptrunc };
						} else {
							// No-op cast
							return SimpleOperation{ OpKind::mov };
						}
					}
					variant_default {
						CORE_PANIC("Unsupported cast from float-layout to target layout");
					}
				}
			}
			variant_default { CORE_PANIC("Unsupported cast source layout in DVM lowering"); }
		}
		CORE_UNREACHABLE();
	}

	DVMOperation lirInstrToDVMOperation(const lir::Instruction& instr) {
		auto operation = instr.operation;
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
		case Call:
			return SimpleOperation{ OpKind::call };

		case Cast: {
			const auto cast_params
				= std::get_if<lir::CastParameters>(&instr.extra_params);
			CORE_ASSERT(cast_params != nullptr, "Cast instruction without parameters");
			return lirCastParamsToDVMOperation(*cast_params);
		}


		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
		CORE_UNREACHABLE();
	}


}
