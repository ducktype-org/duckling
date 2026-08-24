#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <dvm_value.hpp>
#include <function_lowering_context.hpp>

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
                    CORE_PANIC("Unhandled comparison operation in compTimeEvaluateComparison");
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
		// clang-format off
		switch (operation) {
		case OpKind::cmpEq: 	return OpKind::cmpEq;
		case OpKind::cmpNeq: 	return OpKind::cmpNeq;
		case OpKind::fcmpEq: 	return OpKind::fcmpEq;
		case OpKind::fcmpNeq: 	return OpKind::fcmpNeq;
		case OpKind::cmpGt: 	return OpKind::cmpLt;
		case OpKind::cmpGe: 	return OpKind::cmpLe;
		case OpKind::ucmpGt: 	return OpKind::ucmpLt;
		case OpKind::ucmpGe: 	return OpKind::ucmpLe;
		case OpKind::cmpLt: 	return OpKind::cmpGt;
		case OpKind::cmpLe: 	return OpKind::cmpGe;
		case OpKind::fcmpGt: 	return OpKind::fcmpLt;
		case OpKind::fcmpGe: 	return OpKind::fcmpLe;
		case OpKind::fcmpLt: 	return OpKind::fcmpGt;
		case OpKind::fcmpLe: 	return OpKind::fcmpGe;
		default: 				CORE_PANIC("Unhandled comparison operation in getComparisonOppositeDirection");
		}
		// clang-format on
	}
}

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(ComparisonOperation& op) {
		DVMValue result_val = [&]() -> DVMValue {
			// Shortcut for comparing immediates to keep the same semantics as comp-time comparisons.
			if (op.lhs.is<DVMImmediate>() && op.rhs.is<DVMImmediate>()) {
				bool const_result = compTimeEvaluateComparison(op.op, *op.lhs_const, *op.rhs_const);
				return { DVMImmediate::boolean(const_result) };
			}

			if (op.lhs.is<DVMImmediate>()) {
				// Swap arguments to place immediate on the right side.
				std::swap(op.lhs, op.rhs);
				op.op = getComparisonOppositeDirection(op.op);
			}

			// This resolves e.g. `x = a CMP b;`
			// by splitting it into three instructions:
			// a CMP b;
			// mov x, 0;
			// cmov x, 1;
			auto                     result_type = DVMImmediate::u8(u8(0)).type;
			base::Optional<DVMPlace> dest_opt;

			if (op.dest.has_value() && op.dest->isDirect())
				// If the destination is a direct place, we can use it directly without creating a temp.
				dest_opt = op.dest;
			else
				dest_opt = ctx->pushTempLocal(result_type, "cmp_tmp");

			DVMPlace tmp_res = dest_opt.value();

			ctx->pushInstruction({ op.op, op.lhs, op.rhs });
			ctx->pushInstruction({ OpKind::mov, tmp_res, DVMImmediate::u8(u8(0)) });
			ctx->pushInstruction({ OpKind::cmov, tmp_res, DVMImmediate::u8(u8(1)) });
			return { tmp_res };
		}();
		ctx->maybeStoreResult(op.dest, result_val);
	}
}
