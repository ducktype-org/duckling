#include "calling_conv.hpp"

#include "abi/type_system/type.hpp"

#include <abi/layout/compute_c_layout.hpp>

#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"
#include "base/types/bits_and_bytes.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

namespace abi::calling_conv {

	ArgInfo ArgInfo::byPointer(bool by_val) {
		return ArgInfo{ .kind = ByPointer{ .by_val = by_val } };
	}

	ArgInfo ArgInfo::byValue(types::AbiType type, bool sign_ext, bool zero_ext) {
		return ArgInfo{ .kind = ByValue{ .coerce_to_type = std::move(type),
			                             .sign_ext       = sign_ext,
			                             .zero_ext       = zero_ext } };
	}

	std::vector<types::AbiType> flattenType(
		const TargetABI& target, const types::AbiType& type, layout::ComputedLayout& expanded_layout
	) {
		std::vector<types::AbiType> result;
		auto                        size_align = layout::sizeAlignOf(target, type);
		expanded_layout.alignment              = size_align.alignment;
		expanded_layout.size                   = size_align.size;

		auto flatten_rec = [&](auto&& self, const types::AbiType& node, Bytes offset) -> void {
			variant_match(node.value) {
				variant_case(types::IntType, v) {
					result.push_back({ v });
					expanded_layout.field_offsets.push_back(offset);
				}
				variant_case(types::FloatType, v) {
					result.push_back({ v });
					expanded_layout.field_offsets.push_back(offset);
				}
				variant_case(types::BoolType, v) {
					result.push_back(types::intType(8, false));
					expanded_layout.field_offsets.push_back(offset);
				}
				variant_case(types::CharType, v) {
					result.push_back(types::intType(8, false));
					expanded_layout.field_offsets.push_back(offset);
				}
				variant_case(types::PointerType, v) {
					result.push_back(
						types::intType(target.data_layout.pointer_size.asInt() * 8, false)
					);
					expanded_layout.field_offsets.push_back(offset);
				}
				variant_case(types::StructType, v) {
					auto struct_layout = layout::computeCLayout(target, v.fields);
					for (auto&& [field, field_offset]:
					     std::views::zip(v.fields, struct_layout.field_offsets))
						self(self, *field, field_offset);
				}
				variant_case(types::ArrayType, v) {
					auto elem_size_align = layout::sizeAlignOf(target, *v.element);
					for (int i{ 0 }; i < v.count; i++) {
						self(self, *v.element, offset);
						offset += elem_size_align.size;
					}
				}
			}
		};
		flatten_rec(flatten_rec, type, Bytes(0));
		return result;
	}

	namespace {
		Bytes roundUpIntBytes(Bytes bytes) {
			if (bytes <= Bytes(1)) return Bytes(1);
			if (bytes <= Bytes(2)) return Bytes(2);
			if (bytes <= Bytes(4)) return Bytes(4);
			return Bytes(8);
		}
	}

	/**
	 * @brief Combine into struct with two fields
	 * with the algorithm from x86-64 ABI.
	 */
	types::AbiType combineToLowHighStruct(
		const TargetABI&                   target,
		const std::vector<types::AbiType>& flattened,
		const layout::ComputedLayout&      expanded_layout
	) {
		enum class Eightbyte { NoClass, Integer, Sse };

		std::array<Eightbyte, 2> classes = { Eightbyte::NoClass, Eightbyte::NoClass };

		// Floating-point extent within each eightbyte,
		// for example fp_used[1] = 4, means we only used the first 4 bytes of the second register.
		std::array<u64, 2> fp_used = { 0, 0 };

		for (auto&& [offset, leaf]: std::views::zip(expanded_layout.field_offsets, flattened)) {
			u64  elem_bytes = 0;
			bool is_float   = false;
			variant_match(leaf.value) {
				variant_case(types::IntType, v) { elem_bytes = v.width_bits / 8; }
				variant_case(types::FloatType, v) {
					elem_bytes = v.width_bits / 8;
					is_float   = true;
				}
				variant_default { /* flattenType yields only Int/Float leaves */ }
			}
			if (elem_bytes == 0) continue;

			auto idx = offset.asInt() / 8;

			auto elem_desired_type = is_float ? Eightbyte::Sse : Eightbyte::Integer;

			switch (classes.at(idx)) {
			case Eightbyte::NoClass:
			case Eightbyte::Sse:
				classes.at(idx) = elem_desired_type;
				break;
			case Eightbyte::Integer:
				// We do not do anything.
				break;
			}

			if (is_float)
				fp_used.at(idx) = std::max(fp_used.at(idx), (offset.asInt() % 8) + elem_bytes);
		}


		std::vector<types::AbiTypePtr> fields;
		Bytes                          current_offset{ 0 };
		Bytes                          total_size = expanded_layout.field_offsets.back()
		                 + layout::sizeAlignOf(target, flattened.back()).size;
		for (u64 i{ 0 }; i < 2; i++) {
			switch (classes.at(i)) {
			case Eightbyte::Sse: {
				u64 float_width_bits = fp_used.at(i) <= 4 ? 32 : 64;
				fields.push_back(types::makeBoxAbiType(types::floatType(float_width_bits)));
				break;
			}
			case Eightbyte::Integer: {
				// LLVM takes care of rounding the int to the correct size.
				u64 int_with_bits
					= base::bytes2bits(
						  roundUpIntBytes(std::min(total_size - current_offset, Bytes(8)))
					)
				          .asInt();
				fields.push_back(types::makeBoxAbiType(types::intType(int_with_bits, false)));
				break;
			}
			default:
				break;
			}
			current_offset += Bytes(8);
		}
		return types::structType(std::move(fields));
	}

#define ARG_ENTRY(arg_info) \
	ArgEntry { .info = arg_info, .original_type = original_type }
#define RETURN_ENTRY(ret_info, passed_as_param_val)                                              \
	ReturnEntry {                                                                                \
		.info = ret_info, .passed_as_param = passed_as_param_val, .original_type = original_type \
	}

	FunctionInfo X86_64ABIInfo::computeInfo(const FunctionType& ft) const {
		auto compute_arg_entry = [&](const types::AbiTypeCRef& original_type) {
			auto size_align = layout::sizeAlignOf(myTargetABI(), *original_type);

			// Fast path for simple types
			if (v_matches(
					original_type->value,
					types::IntType,
					types::FloatType,
					types::CharType,
					types::BoolType,
					types::PointerType
				))
				return ARG_ENTRY(ArgInfo::byValue(types::cloneAbiType(*original_type)));

			if (size_align.size <= Bytes(16)) {
				layout::ComputedLayout expanded_layout;
				auto flattened_types = flattenType(myTargetABI(), *original_type, expanded_layout);
				auto type = combineToLowHighStruct(myTargetABI(), flattened_types, expanded_layout);
				return ARG_ENTRY(ArgInfo::byValue(std::move(type)));
			} else {
				return ARG_ENTRY(ArgInfo::byPointer(true));
			}
		};
		auto compute_return_entry = [&](const types::AbiTypeCRef& original_type) {
			auto arg_info        = compute_arg_entry(original_type);
			bool passed_as_param = false;
			if (auto by_pointer = std::get_if<ArgInfo::ByPointer>(&arg_info.info.kind)) {
				passed_as_param    = true;
				by_pointer->by_val = false;
			}
			return RETURN_ENTRY(std::move(arg_info.info), passed_as_param);
		};
		return FunctionInfo{ .return_info = compute_return_entry(ft.return_type),
			                 .param_info = ft.param_types | std::views::transform(compute_arg_entry)
			                             | std::ranges::to<std::vector>() };
	}

	/**
	 * @brief Is homogeneous floating-point aggregate" (HFA) for Aarch64.
	 */
	bool isHomogeneous(const std::vector<types::AbiType>& types) {
		if (types.size() > 4 or types.size() == 0) return false;

		auto& first = types.at(0);
		if (not base::holds<types::FloatType>(first.value)) return false;
		for (u64 i{ 1 }; i < types.size(); i++)
			if (first != types.at(i)) return false;
		return true;
	}

	FunctionInfo AArch64ABIInfo::computeInfo(const FunctionType& ft) const {
		auto homogeneous_arg_info = [&](std::vector<types::AbiType> types) {
			std::vector<types::AbiTypePtr> fields;
			fields.reserve(types.size());
			for (auto&& type: types) fields.emplace_back(types::makeBoxAbiType(std::move(type)));
			return ArgInfo::byValue(types::structType(std::move(fields)));
		};
		auto two_words_arg_info = [&]() {
			std::vector<types::AbiTypePtr> fields;
			fields.push_back(types::makeBoxAbiType(types::intType(64, false)));
			fields.push_back(types::makeBoxAbiType(types::intType(64, false)));
			return ArgInfo::byValue(types::structType(std::move(fields)));
		};

		auto compute_arg_entry = [&](const types::AbiTypeCRef& original_type) -> ArgEntry {
			// Fast path for simple types
			if (v_matches(
					original_type->value,
					types::IntType,
					types::FloatType,
					types::CharType,
					types::BoolType,
					types::PointerType
				))
				return ARG_ENTRY(ArgInfo::byValue(types::cloneAbiType(*original_type)));

			[[maybe_unused]] layout::ComputedLayout computed_layout;
			auto flattened_types = flattenType(myTargetABI(), *original_type, computed_layout);
			if (isHomogeneous(flattened_types))
				return ARG_ENTRY(homogeneous_arg_info(std::move(flattened_types)));

			if (computed_layout.size <= Bytes(8))
				return ARG_ENTRY(ArgInfo::byValue(types::intType(64, false)));
			else if (computed_layout.size <= Bytes(16))
				return ARG_ENTRY(two_words_arg_info());
			else
				return ARG_ENTRY(ArgInfo::byPointer(false));
		};

		auto compute_return_entry = [&](const types::AbiTypeCRef& original_type) -> ReturnEntry {
			// Fast path for simple types
			if (v_matches(
					original_type->value,
					types::IntType,
					types::FloatType,
					types::CharType,
					types::BoolType,
					types::PointerType
				))
				return RETURN_ENTRY(ArgInfo::byValue(types::cloneAbiType(*original_type)), false);


			[[maybe_unused]] layout::ComputedLayout computed_layout;
			auto flattened_types = flattenType(myTargetABI(), *original_type, computed_layout);
			if (isHomogeneous(flattened_types))
				return RETURN_ENTRY(homogeneous_arg_info(std::move(flattened_types)), false);

			if (computed_layout.size <= Bytes(8)) {
				return RETURN_ENTRY(
					ArgInfo::byValue(
						types::intType(base::bytes2bits(computed_layout.size).asInt(), false)
					),
					false
				);
			} else if (computed_layout.size <= Bytes(16)) {
				return RETURN_ENTRY(two_words_arg_info(), false);
			} else {
				return RETURN_ENTRY(ArgInfo::byPointer(false), true);
			}
		};


		return FunctionInfo{ .return_info = compute_return_entry(ft.return_type),
			                 .param_info = ft.param_types | std::views::transform(compute_arg_entry)
			                             | std::ranges::to<std::vector>() };
	}

	FunctionInfo computeCallingConv(TargetABI& target, const FunctionType& ft) {
		switch (target.triple.arch) {
		case Arch::X86_64:
			return X86_64ABIInfo{}.computeInfo(ft);
		case Arch::AArch64:
			return AArch64ABIInfo{}.computeInfo(ft);
		default:
			CORE_UNREACHABLE();
		}
	}
}
