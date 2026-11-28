// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file extc_function.hpp
 * @brief Registers plain C++ functions as external C functions (see `extc.hpp`).
 *
 * ```cpp
 * i64 add(i64 a, i64 b) { return a + b; }
 * i64 depth(vm::ExtCContext& ctx, Tree* tree);                 // optional leading context
 * std::tuple<i64, i64> divMod(i64 a, i64 b);                  // several results
 * std::expected<i64, vm::ExtCError> parse(Text* text);        // failure becomes a VM panic
 * void show(vm::extc::Value<"string"> text);                  // any VM type, as a VMValue
 *
 * .external_c_functions = { VM_EXT_C_FUNC(add), VM_EXT_C_FUNC_AS("div_mod", divMod) }
 * ```
 *
 * The VM types come from the C++ types through `VmType`: integers map to the VM integer of the same
 * size, `bool` to `i8`, `f32`/`f64` to the types of the same names, any `T*` to `opaque_ptr`.
 * Specialise `VmType` to map more types. The loader checks that each mapped VM type exists and has
 * the size of its C++ type.
 *
 * A function uses the raw convention, unless a parameter is a `vm::extc::Value`, which needs the
 * value convention. The raw convention only allows trivially copyable VM types, which the loader
 * checks.
 *
 * @note Overloaded functions need an explicit cast to the wanted overload.
 */
#pragma once

#include <base/comptime/template_string.hpp>
#include <base/types/floats.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/extc/extc.hpp>
#include <vm/utils/interpret.hpp>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace vm::extc {
	/** @brief A parameter of any VM type `NAME`, passed as the VMValue itself. */
	template<base::TemplateStringLiteral NAME>
	struct Value final {
		const IVMValue& value;
	};

	/**
	 * @brief The VM type of a C++ type: `NAME` and the C++ `SIZE` the loader checks it against
	 * (0 skips the check).
	 */
	template<class T>
	struct VmType final {
		static_assert(
			sizeof(T) == 0,
			"This C++ type has no VM type. Use a mapped type, `vm::extc::Value` or specialise "
			"`vm::extc::VmType`."
		);
	};

	namespace detail {
		template<usize SIZE>
		consteval const char* integerTypeName() {
			if constexpr (SIZE == 1)
				return "i8";
			else if constexpr (SIZE == 2)
				return "i16";
			else if constexpr (SIZE == 4)
				return "i32";
			else if constexpr (SIZE == 8)
				return "i64";
			else
				static_assert(SIZE == 0, "No VM integer type of this size.");
		}
	}

	template<class T>
	requires(std::is_integral_v<T> && !std::is_same_v<T, bool>) struct VmType<T> final {
		static constexpr const char* NAME = detail::integerTypeName<sizeof(T)>();
		static constexpr usize       SIZE = sizeof(T);
	};

	template<>
	struct VmType<i8> final {
		static constexpr const char* NAME = "i8";
		static constexpr usize       SIZE = 1;
	};

	template<>
	struct VmType<u8> final {
		static constexpr const char* NAME = "i8";
		static constexpr usize       SIZE = 1;
	};

	template<>
	struct VmType<bool> final {
		static constexpr const char* NAME = "i8";
		static constexpr usize       SIZE = 1;
	};

	template<>
	struct VmType<f32> final {
		static constexpr const char* NAME = "f32";
		static constexpr usize       SIZE = sizeof(f32);
	};

	template<>
	struct VmType<f64> final {
		static constexpr const char* NAME = "f64";
		static constexpr usize       SIZE = sizeof(f64);
	};

	template<class T>
	struct VmType<T*> final {
		static constexpr const char* NAME = "opaque_ptr";
		static constexpr usize       SIZE = sizeof(T*);
	};

	template<base::TemplateStringLiteral NAME_LITERAL>
	struct VmType<Value<NAME_LITERAL>> final {
		static constexpr const char* NAME = NAME_LITERAL.value;
		static constexpr usize       SIZE = 0;
	};

	namespace detail {
		template<class T>
		struct IsValue final: std::false_type {};

		template<base::TemplateStringLiteral NAME>
		struct IsValue<Value<NAME>> final: std::true_type {};

		template<class T>
		struct IsExpected final: std::false_type {};

		template<class T>
		struct IsExpected<std::expected<T, ExtCError>> final: std::true_type {};

		template<class T>
		struct IsTuple final: std::false_type {};

		template<class... TS>
		struct IsTuple<std::tuple<TS...>> final: std::true_type {};

		template<class F>
		struct FunctionTraits;

		template<class R, class... PS>
		struct FunctionTraits<R (*)(PS...)> {
			using Result = R;
			using Params = std::tuple<PS...>;
		};

		template<class R, class... PS>
		struct FunctionTraits<R (*)(PS...) noexcept>: FunctionTraits<R (*)(PS...)> {};

		/// Splits off a leading `ExtCContext&` parameter.
		template<class PARAMS>
		struct ContextParam {
			static constexpr bool TAKES_CONTEXT = false;
			using VmParams                      = PARAMS;
		};

		template<class... REST>
		struct ContextParam<std::tuple<ExtCContext&, REST...>> {
			static constexpr bool TAKES_CONTEXT = true;
			using VmParams                      = std::tuple<REST...>;
		};

		/// The VM results of a C++ result type, as a tuple of plain types.
		template<class R>
		struct VmResults {
			using Types = std::tuple<R>;
		};

		template<>
		struct VmResults<void> {
			using Types = std::tuple<>;
		};

		template<class... TS>
		struct VmResults<std::tuple<TS...>> {
			using Types = std::tuple<TS...>;
		};

		template<class T>
		struct VmResults<std::expected<T, ExtCError>>: VmResults<T> {};

		template<class TUPLE>
		struct TupleSignature;

		template<class... TS>
		struct TupleSignature<std::tuple<TS...>> {
			static std::vector<code::Identifier> names() {
				return { code::Identifier(base::StrID(VmType<std::remove_cvref_t<TS>>::NAME))... };
			}

			static std::vector<usize> sizes() {
				return { VmType<std::remove_cvref_t<TS>>::SIZE... };
			}

			/// Whether any element is a `Value`, which needs the value convention.
			static constexpr bool ANY_VALUE = (IsValue<std::remove_cvref_t<TS>>::value || ...);

			/// Whether every non-`Value` element has the size its `VmType` declares, which the
			/// loader checks against the VM type.
			static constexpr bool SIZES_MATCH
				= ((IsValue<std::remove_cvref_t<TS>>::value
			        || VmType<std::remove_cvref_t<TS>>::SIZE == sizeof(std::remove_cvref_t<TS>))
			       && ...);
		};

		template<class T>
		T decodeArg(const IVMValue& value) {
			if constexpr (IsValue<T>::value)
				return T{ value };
			else if constexpr (std::is_same_v<T, bool>)
				return value.readBytes<uint8_t>() != 0;
			else
				return value.readBytes<T>();
		}

		inline void freeResults(ExtCContext& ctx, ExtCResults& results) {
			for (auto& result: results) ctx.freeValue(std::move(result));
			results.clear();
		}

		template<class T>
		bool encodeResult(ExtCContext& ctx, const T& value, ExtCResults& results, ExtCError& error) {
			static_assert(!IsValue<T>::value, "`vm::extc::Value` can only be a parameter.");
			auto result = ctx.createResult(results.size());
			if (!result.has_value()) {
				error = std::move(result.error());
				return false;
			}
			if constexpr (std::is_same_v<T, bool>)
				(*result)->writeBytes<uint8_t>(value ? 1 : 0);
			else
				(*result)->writeBytes<T>(value);
			results.push_back(std::move(*result));
			return true;
		}

		/// Turns the C++ result into VMValues, freeing the ones made so far on a failure.
		template<class R>
		ExtCReturn encodeResults(ExtCContext& ctx, R&& result) {
			using Plain = std::remove_cvref_t<R>;
			if constexpr (IsExpected<Plain>::value) {
				if (!result.has_value()) return std::unexpected(std::move(result.error()));
				if constexpr (std::is_void_v<typename Plain::value_type>)
					return ExtCResults{};
				else
					return encodeResults(ctx, std::move(*result));
			} else {
				ExtCResults results;
				ExtCError   error;
				bool        encoded = true;
				if constexpr (IsTuple<Plain>::value) {
					std::apply(
						[&](const auto&... values) {
							encoded = (encodeResult(ctx, values, results, error) && ...);
						},
						result
					);
				} else {
					encoded = encodeResult(ctx, result, results, error);
				}
				if (!encoded) {
					freeResults(ctx, results);
					return std::unexpected(std::move(error));
				}
				return results;
			}
		}

		template<class T>
		T decodeRawArg(const byte* data) {
			if constexpr (std::is_same_v<T, bool>)
				return vm::safeReadPointerBytes<uint8_t>(data) != 0;
			else
				return vm::safeReadPointerBytes<T>(data);
		}

		template<class T>
		void encodeRawResult(byte* data, const T& value) {
			if constexpr (std::is_same_v<T, bool>)
				vm::safeWriteBytes<uint8_t>(data, value ? 1 : 0);
			else
				vm::safeWriteBytes<T>(data, value);
		}

		/// Writes the C++ result into the result slots.
		template<class R>
		ExtCRawReturn encodeRawResults(ExtCRawResults results, R&& result) {
			using Plain = std::remove_cvref_t<R>;
			if constexpr (IsExpected<Plain>::value) {
				if (!result.has_value()) return std::unexpected(std::move(result.error()));
				if constexpr (std::is_void_v<typename Plain::value_type>)
					return {};
				else
					return encodeRawResults(results, std::move(*result));
			} else if constexpr (IsTuple<Plain>::value) {
				std::apply(
					[&](const auto&... values) {
						usize index = 0;
						(encodeRawResult(results[index++], values), ...);
					},
					result
				);
				return {};
			} else {
				encodeRawResult(results[0], result);
				return {};
			}
		}

		/// Calls `F` with its leading context, if it takes one, and `args`.
		template<auto F, class... ARGS>
		decltype(auto) callWithContext(ExtCContext& ctx, ARGS&&... args) {
			using Traits = FunctionTraits<decltype(F)>;
			if constexpr (ContextParam<typename Traits::Params>::TAKES_CONTEXT)
				return F(ctx, std::forward<ARGS>(args)...);
			else
				return F(std::forward<ARGS>(args)...);
		}

		template<auto F>
		ExtCRawReturn rawWrapper(ExtCContext& ctx, ExtCRawArgs args, ExtCRawResults results) {
			using Traits   = FunctionTraits<decltype(F)>;
			using VmParams = typename ContextParam<typename Traits::Params>::VmParams;

			auto call = [&]<usize... IS>(std::index_sequence<IS...>) -> decltype(auto) {
				return callWithContext<F>(
					ctx,
					decodeRawArg<std::remove_cvref_t<std::tuple_element_t<IS, VmParams>>>(args[IS]
				    )...
				);
			};
			auto indices = std::make_index_sequence<std::tuple_size_v<VmParams>>();

			if constexpr (std::is_void_v<typename Traits::Result>) {
				call(indices);
				return {};
			} else {
				return encodeRawResults(results, call(indices));
			}
		}

		template<auto F>
		ExtCReturn valueWrapper(ExtCContext& ctx, ExtCArgs args) {
			using Traits   = FunctionTraits<decltype(F)>;
			using VmParams = typename ContextParam<typename Traits::Params>::VmParams;

			auto call = [&]<usize... IS>(std::index_sequence<IS...>) -> decltype(auto) {
				return callWithContext<F>(
					ctx,
					decodeArg<std::remove_cvref_t<std::tuple_element_t<IS, VmParams>>>(*args[IS])...
				);
			};
			auto indices = std::make_index_sequence<std::tuple_size_v<VmParams>>();

			if constexpr (std::is_void_v<typename Traits::Result>) {
				call(indices);
				return ExtCResults{};
			} else {
				return encodeResults(ctx, call(indices));
			}
		}
	}

	/**
	 * @brief Makes an external C function from the C++ function `F`, registered as `name`. It uses
	 * the raw convention, unless a parameter is a `Value`.
	 * @param user_data What `ExtCContext::userData` returns during the calls.
	 */
	template<auto F>
	code::ExternalCFunction function(base::StrID name, void* user_data = nullptr) {
		using Traits   = detail::FunctionTraits<decltype(F)>;
		using VmParams = typename detail::ContextParam<typename Traits::Params>::VmParams;
		using Results  = typename detail::VmResults<typename Traits::Result>::Types;
		static_assert(
			detail::TupleSignature<VmParams>::SIZES_MATCH
				&& detail::TupleSignature<Results>::SIZES_MATCH,
			"A `vm::extc::VmType` declares a size different from its C++ type."
		);
		static_assert(
			!detail::TupleSignature<Results>::ANY_VALUE, "`vm::extc::Value` can only be a parameter."
		);

		ExtCFunction pointer{};
		if constexpr (detail::TupleSignature<VmParams>::ANY_VALUE)
			pointer = &detail::valueWrapper<F>;
		else
			pointer = &detail::rawWrapper<F>;

		return code::ExternalCFunction{
			.name            = code::Identifier(name),
			.function        = pointer,
			.user_data       = user_data,
			.signature       = { .result_types = detail::TupleSignature<Results>::names(),
			                     .parameters   = detail::TupleSignature<VmParams>::names() },
			.parameter_sizes = detail::TupleSignature<VmParams>::sizes(),
			.result_sizes    = detail::TupleSignature<Results>::sizes(),
		};
	}

	/** @brief The last component of a qualified C++ name, e.g. `f` for `ns::f`. */
	inline base::StrID unqualifiedName(std::string_view name) {
		const auto separator = name.rfind("::");
		if (separator != std::string_view::npos) name.remove_prefix(separator + 2);
		return base::StrID(std::string(name));
	}
}

/// Registers the C++ function `FUNC` under its unqualified name.
#define VM_EXT_C_FUNC(FUNC) ::vm::extc::function<&FUNC>(::vm::extc::unqualifiedName(#FUNC))

/// Registers the C++ function `FUNC` under the name `VM_NAME`.
#define VM_EXT_C_FUNC_AS(VM_NAME, FUNC) ::vm::extc::function<&FUNC>(::base::StrID(VM_NAME))
