#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

namespace {
	using namespace compiler;
	using namespace vm::code::builders;

	/**
	 * @brief Whether the cast targets `bool`, which is lowered as a "is the source non-zero" test
	 * rather than as a truncation.
	 */
	bool castsToBool(const lir::CastParameters& cast_params) {
		return cast_params.target_type.getType().getKind() == tsh::Kind::Bool;
	}

	/**
	 * @brief Whether the cast narrows a `manyptr T` down to a `ptr T`, which on the DVM means
	 * taking the address of the first element of the dynamic table behind the many-pointer.
	 */
	bool castsManyPointerToPointer(const lir::CastParameters& cast_params) {
		using tsl::PointerTypeLayout;
		const auto& source_variant = cast_params.source_layout->getVariant();
		const auto& target_variant = cast_params.target_layout->getVariant();
		if (not std::holds_alternative<PointerTypeLayout>(source_variant)
		    or not std::holds_alternative<PointerTypeLayout>(target_variant))
			return false;
		return std::get<PointerTypeLayout>(source_variant).getPointerKind()
		        == PointerTypeLayout::PointerKind::ManyPointer
		   and std::get<PointerTypeLayout>(target_variant).getPointerKind()
		           == PointerTypeLayout::PointerKind::SinglePointer;
	}

	/**
	 * @brief The comparison used to turn a numeric source value into a `bool`.
	 */
	OpKind getNonZeroComparison(const lir::CastParameters& cast_params) {
		if (cast_params.source_layout->is<tsl::FloatTypeLayout>()) return OpKind::fcmpNeq;
		CORE_ASSERT(
			cast_params.source_layout->is<tsl::IntegralTypeLayout>(),
			"Only integral and float sources can be cast to bool"
		);
		return OpKind::cmpNeq;
	}

	vm::code::builders::OpKind getOpKindFromLIRLayouts(const lir::CastParameters& cast_params) {
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
							return src_signed ? OpKind::sext : OpKind::zext;
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return OpKind::trunc;
						} else {
							// No-op cast
							return OpKind::mov;
						}
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Int -> Float ==================
						return src_signed ? OpKind::sitofp : OpKind::uitofp;
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
						return to_signed ? OpKind::fptosi : OpKind::fptoui;
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Float -> Float ==================
						if (source_layout->getSize() < target_layout->getSize()) {
							// Extend
							return OpKind::fpext;
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return OpKind::fptrunc;
						} else {
							// No-op cast
							return OpKind::mov;
						}
					}
					variant_default {
						CORE_PANIC("Unsupported cast from float-layout to target layout");
					}
				}
			}
			variant_case(tsl::PointerTypeLayout, pointer_layout) {
				variant_match(target_layout->getVariant()) {
					variant_case(tsl::PointerTypeLayout, target_pointer_layout) {
						using tsl::PointerTypeLayout::PointerKind::CPointer;
						using tsl::PointerTypeLayout::PointerKind::ManyPointer;
						using tsl::PointerTypeLayout::PointerKind::SinglePointer;
						// Pointer -> CPointer
						if ((pointer_layout.getPointerKind() == SinglePointer
						     || pointer_layout.getPointerKind() == ManyPointer)
						    && target_pointer_layout.getPointerKind() == CPointer) {
							// @TODO: #2745 add support for this cast
							CORE_PANIC("Casting to CPointer is not supported yet");
						} else {
							CORE_PANIC("Unsupported cast between different pointer kinds");
						}
					}
					variant_default {
						CORE_PANIC("Unsupported cast from pointer-layout to target layout");
					}
				}
			}
			variant_default {
				CORE_PANIC(base::strConcat(
					"Unsupported cast source layout in DVM lowering: ",
					source_layout->toStringIdentification()
				));
			}
		}
		CORE_UNREACHABLE();
	}
}

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const CastOperation& op) {
		auto target_type = **ctx->program_context.lowerAndKeepTslType(op.cast_params.target_layout);

		// If the source is an immediate, we place it in a local and perform a cast on it.
		DVMPlace src_arg = ctx->forceToPlace(op.src, "cast_src_tmp");

		if (castsToBool(op.cast_params)) {
			// A `bool` only holds 0 or 1, so a numeric source is compared against zero instead of
			// being truncated (`64 as bool` is `true`, not `false`). Mirrors how a comparison
			// operation is lowered: compare, then materialise the flag with `mov` + `cmov`.
			const DVMPlace result = (op.dest && op.dest->isDirect())
			                          ? *op.dest
			                          : ctx->pushTempLocal(target_type, "cast_bool_tmp");

			ctx->pushInstruction({ getNonZeroComparison(op.cast_params),
			                       src_arg,
			                       DVMImmediate(0, src_arg.getType()) });
			ctx->pushInstruction({ OpKind::mov, result, DVMImmediate::u8(u8(0)) });
			ctx->pushInstruction({ OpKind::cmov, result, DVMImmediate::u8(u8(1)) });
			ctx->maybeStoreResult(op.dest, { result });
			return;
		}

		if (castsManyPointerToPointer(op.cast_params)) {
			// A `manyptr T` is a pointer to a dynamic table of `T`, so narrowing it to a `ptr T`
			// means taking the address of the table's first element.
			const DVMPlace result = (op.dest && op.dest->isDirect())
			                          ? *op.dest
			                          : ctx->pushTempLocal(target_type, "cast_dst_tmp");

			// `dynTableLea` only takes the index as a place, so the constant 0 needs a temporary.
			const auto     zero_index = DVMImmediate::u64(u64(0));
			const DVMPlace index_tmp  = ctx->pushTempLocal(zero_index.type, "cast_index_tmp");
			ctx->pushInstruction({ OpKind::mov, index_tmp, zero_index });
			ctx->pushInstruction({ OpKind::dynTableLea, result, src_arg, index_tmp });
			ctx->maybeStoreResult(op.dest, { result });
			return;
		}

		// Operation in form a = OP b (like mov)
		auto operation = getOpKindFromLIRLayouts(op.cast_params);

		if (op.dest && op.dest->isDirect()) {
			// If output is a direct (not a local storing a pointer to the output place) local,
			// we optimize the cast to work directly on the local.
			ctx->pushInstruction({ operation, *op.dest, src_arg });
		} else {
			// Otherwise, if the output place is not direct, we have to create a
			// temporary to place the cast result and then move it into the output place.
			DVMPlace dst_temp = ctx->pushTempLocal(target_type, "cast_dst_tmp");
			ctx->pushInstruction({ operation, dst_temp, src_arg });
			ctx->maybeStoreResult(op.dest, { dst_temp });
		}
	}
}
