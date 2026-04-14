#include "comparison_operation_lowering.hpp"

#include "dvm_operation.hpp"
#include "function_lowering_context.hpp"
#include "lir/lir_structure/lir_structure.hpp"

namespace {
	using namespace compiler;
	using namespace vm::code::builders;

	/**
	 * @brief Helper used to evaluate comparison operations at compile-time.
	 * @note Keep the same semantics as in the compiler's comp_time and VM.
	 * @return The result of the comparison.
	 */
	bool compTimeEvaluateComparison(
		OpKind                       operation,
		const ctv::CompileTimeValue& lhs_value,
		const ctv::CompileTimeValue& rhs_value
	) {
		auto lhs_numeric_opt = lhs_value.get<ctv::NumericValue>();
		auto rhs_numeric_opt = rhs_value.get<ctv::NumericValue>();
		CORE_ASSERT(
			lhs_numeric_opt.has_value() && rhs_numeric_opt.has_value(),
			"Comparison between non-numeric immediates is not supported"
		);
		auto lhs_numeric = lhs_numeric_opt.value();
		auto rhs_numeric = rhs_numeric_opt.value();
		auto result      = std::visit(
			[&](auto&& lhs_num) -> bool {
				using LhsNumT    = std::decay_t<decltype(lhs_num)>;
				auto rhs_num_opt = rhs_numeric.template get<LhsNumT>();
				CORE_ASSERT(
					rhs_num_opt.has_value(),
					"Comparison between different numeric types is not supported"
				);
				auto rhs_num = rhs_num_opt.value();
				switch (operation) {
				case OpKind::cmpEq:
				case OpKind::fcmpEq:
					return lhs_num == rhs_num;
				case OpKind::cmpNeq:
				case OpKind::fcmpNeq:
					return lhs_num != rhs_num;
				case OpKind::cmpGt:
				case OpKind::fcmpGt:
				case OpKind::ucmpGt:
					return lhs_num > rhs_num;
				case OpKind::cmpGe:
				case OpKind::fcmpGe:
				case OpKind::ucmpGe:
					return lhs_num >= rhs_num;
				case OpKind::cmpLt:
				case OpKind::fcmpLt:
				case OpKind::ucmpLt:
					return lhs_num < rhs_num;
				case OpKind::cmpLe:
				case OpKind::fcmpLe:
				case OpKind::ucmpLe:
					return lhs_num <= rhs_num;
				default:
					CORE_PANIC("Unhandled comparison operation");
				}
			},
			lhs_numeric.getStorage()
		);
		return result;
	}

	/**
	 * @brief Gets the opposite direction of a comparison operation, e.g `a < b` becomes `b > a`.
	 */
	OpKind getComparisonOppositeDirection(OpKind operation) {
		switch (operation) {
		case OpKind::cmpEq:
			return OpKind::cmpEq;
		case OpKind::cmpNeq:
			return OpKind::cmpNeq;
		case OpKind::fcmpEq:
			return OpKind::fcmpEq;
		case OpKind::fcmpNeq:
			return OpKind::fcmpNeq;
		case OpKind::cmpGt:
			return OpKind::cmpLt;
		case OpKind::cmpGe:
			return OpKind::cmpLe;
		case OpKind::ucmpGt:
			return OpKind::ucmpLt;
		case OpKind::ucmpGe:
			return OpKind::ucmpLe;
		case OpKind::cmpLt:
			return OpKind::cmpGt;
		case OpKind::cmpLe:
			return OpKind::cmpGe;
		case OpKind::fcmpGt:
			return OpKind::fcmpLt;
		case OpKind::fcmpGe:
			return OpKind::fcmpLe;
		case OpKind::fcmpLt:
			return OpKind::fcmpGt;
		case OpKind::fcmpLe:
			return OpKind::fcmpGe;
		default:
			CORE_PANIC("Unhandled comparison operation");
		}
	}

}

namespace compiler::backend_vm::internal {
	void ComparisonOperationLowerer::lower(
		FunctionLoweringContext&    ctx,
		const ComparisonOperation&  comparison_operation,
		std::deque<DVMValue>& args,
		const DVMPlace&             output,
		const lir::Instruction&     lir_instruction
	) {
		CORE_ASSERT(args.size() == 2, "Invalid comparison argument count");
		OpKind operation = comparison_operation.op;

		DVMValue result_val = [&]() -> DVMValue {
			if (args[0].is<DVMImmediate>() && args[1].is<DVMImmediate>()) {
				auto lhs_value = lir_instruction.arguments[0].get<lir::LIRConstant>().value;
				auto rhs_value = lir_instruction.arguments[1].get<lir::LIRConstant>().value;
				auto res       = compTimeEvaluateComparison(operation, lhs_value, rhs_value);
				return { DVMImmediate::boolean(res) };
			} else {
				if (args[0].is<DVMImmediate>()) {
					// Swap arguments to place immediate on the right side.
					std::swap(args[0], args[1]);
					operation = getComparisonOppositeDirection(operation);
				}

				// Force globals into locals if needed.
				auto lhs = ctx.forceToLocal(args[0], "lhs_temp");
				auto rhs = args[1].is<DVMGlobal>()
				             ? DVMValue{ ctx.forceToLocal(args[1], "rhs_temp"),
					                     DVMPlace::AccessKind::Direct }
				             : args[1];

				// This resolves e.g. `x = a CMP b;`
				// by splitting it into three instructions:
				// a CMP b;
				// mov x, 0;
				// cmov x, 1;
				auto tmp_res
					= ctx.pushTempLocal(vm::code::PrimitiveType(base::StrID("i8"), 1), "cnp_tmp");
				ctx.pushInstruction({ operation, lhs, rhs });
				ctx.pushInstruction({ OpKind::mov, tmp_res, DVMImmediate::u8(u8(0)) });
				ctx.pushInstruction({ OpKind::cmov, tmp_res, DVMImmediate::u8(u8(1)) });
				return { tmp_res, DVMPlace::AccessKind::Direct };
			}
		}();

		ctx.storeResult(output, result_val);
	}

}
