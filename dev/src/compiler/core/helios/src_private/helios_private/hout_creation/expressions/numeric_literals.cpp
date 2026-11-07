// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "numeric_literals.hpp"

#include "errors.hpp"

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <diagnostic/stable_position.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

// Floating-point `std::from_chars` is implemented in the libc++ dylib, which macOS has from 26.0 only:
// libc++ marks it unavailable for older deployment targets (the macOS release supports 15). There,
// `parseF64` below uses `strtod_l` in the "C" locale instead. Can be set to 1 to test that on Linux.
#ifndef DUCKLING_FLOAT_FROM_CHARS_FALLBACK
#	if defined(_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT) && !_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT
#		define DUCKLING_FLOAT_FROM_CHARS_FALLBACK 1
#	else
#		define DUCKLING_FLOAT_FROM_CHARS_FALLBACK 0
#	endif
#endif

#if DUCKLING_FLOAT_FROM_CHARS_FALLBACK
#	include <cerrno>
#	include <cmath>
#	include <cstdlib>
#	include <locale.h>
#	ifdef __APPLE__
#		include <xlocale.h>
#	endif
#endif

namespace compiler::helios::code {
	namespace {
		/**
		 * @brief `std::from_chars(first, last, out)` for an `f64` in the default (general) format,
		 * with its results, also where `strtod_l` replaces it (see DUCKLING_FLOAT_FROM_CHARS_FALLBACK).
		 */
		std::from_chars_result parseF64(std::string_view value, f64& out) {
#if DUCKLING_FLOAT_FROM_CHARS_FALLBACK
			const char* const first = value.data();
			// What strtod accepts but from_chars does not: leading whitespace and '+'.
			if (value.empty() || value.front() == '+' || std::isspace(static_cast<unsigned char>(value.front())))
				return { first, std::errc::invalid_argument };
			// Hexadecimal floats: from_chars (general format) reads only the "0" before the 'x'.
			const usize sign = value.front() == '-' ? 1 : 0;
			if (value.substr(sign).starts_with("0x") || value.substr(sign).starts_with("0X")) {
				out = sign ? -0.0 : 0.0;
				return { first + sign + 1, std::errc() };
			}

			static const locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
			const std::string     terminated(value);  // strtod needs a NUL-terminated string.
			char*                 end = nullptr;
			errno                     = 0;
			const f64   parsed        = strtod_l(terminated.c_str(), &end, c_locale);
			const char* ptr           = first + (end - terminated.c_str());
			if (ptr == first) return { first, std::errc::invalid_argument };
			// Overflow (inf) and underflow to zero are out of range; subnormal results are not.
			if (errno == ERANGE && (std::isinf(parsed) || parsed == 0.0))
				return { ptr, std::errc::result_out_of_range };
			out = parsed;
			return { ptr, std::errc() };
#else
			return std::from_chars(value.data(), value.data() + value.size(), out);
#endif
		}

		/**
		 * @brief Checks the given `from_chars_result` and logs compiler errors in case of errors.
		 * @return True `from_chars` succeeded, false if an error occurred, an appropriate compiler
		 * error is logged.
		 */
		bool handleFromCharsFailure(
			const std::from_chars_result& result,
			std::string_view              value,
			const dia::StablePosition&    position,
			query::Context&               ctx
		) {
			if (result.ec != std::errc()) {
				if (result.ec
				    == std::errc::invalid_argument) {  // Not a number at all. This will be returned
					                                   // when trying to parse "abc".
					ctx.log<InvalidNumericLiteralError>(position);
				} else if (result.ec == std::errc::result_out_of_range) {
					ctx.log<NumericLiteralTooLargeError>(position);
				}
				return false;
			}

			// Checks if the whole number was parsed. For example for a literal like this "123abc"
			// `from_chars` won't return the `std::errc::invalid_argument`, but return a parsed
			// "123" literal and stop on the first non numeric char. Here we check that the whole
			// string was parsed.
			if (result.ptr != value.data() + value.size()) {
				ctx.log<InvalidNumericLiteralError>(position);
				return false;
			}
			return true;
		}

		template<typename TargetInt>
		base::Optional<numeric_value::NumericValue> parseSignedInteger(
			std::string_view value, int base, const dia::StablePosition& position, query::Context& ctx
		) {
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};

			if (!base::fitsIn<TargetInt>(parsed_value)) {
				ctx.log<LiteralDoesNotFitError>(position, "signed integer type");
				return {};
			}

			return numeric_value::NumericValue(static_cast<TargetInt>(parsed_value));
		}

		template<typename TargetUInt>
		base::Optional<numeric_value::NumericValue> parseUnsignedInteger(
			std::string_view value, int base, const dia::StablePosition& position, query::Context& ctx
		) {
			u64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			if (!base::fitsIn<TargetUInt>(parsed_value)) {
				ctx.log<LiteralDoesNotFitError>(position, "unsigned integer type");
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetUInt>(parsed_value));
		}

		template<typename TargetFloat>
		base::Optional<numeric_value::NumericValue> parseFloat(
			std::string_view value, const dia::StablePosition& position, query::Context& ctx
		) {
			f64  parsed_value = 0;
			auto result = parseF64(value, parsed_value);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			// @note: No need to check with fitsIn, since casting to smaller types is always okay.
			return numeric_value::NumericValue(static_cast<TargetFloat>(parsed_value));
		}

		template<typename ParsedInt>
		base::Optional<numeric_value::NumericValue> deduceIntegerTypeAs(
			std::string_view value, int base, const dia::StablePosition& position, query::Context& ctx
		) {
			ParsedInt parsed_value = 0;
			auto      result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);
			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			return numeric_value::NumericValue::createMinimized(parsed_value);
		}

		base::Optional<numeric_value::NumericValue> deduceIntegerType(
			std::string_view value, int base, const dia::StablePosition& position, query::Context& ctx
		) {
			if (value.starts_with('-')) return deduceIntegerTypeAs<i64>(value, base, position, ctx);
			return deduceIntegerTypeAs<u64>(value, base, position, ctx);
		}

		base::Optional<numeric_value::NumericValue> deduceFloatType(
			std::string_view value, const dia::StablePosition& position, query::Context& ctx
		) {
			f64  parsed_value = 0;
			auto result = parseF64(value, parsed_value);
			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			return numeric_value::NumericValue::createMinimized(parsed_value);
		}
	}

	bool canBeNegative(pst::Access<pst::expr::ExprNumericValue> literal_expr) {
		using enum lang_def::NumericLiteralTypeSpecifier;
		switch (literal_expr->getTypeSpecifier()) {
		case u8:
		case u16:
		case u32:
		case u64:
		case u128:
			return false;
		default:
			return true;
		}
	}

	base::Optional<compiler::numeric_value::NumericValue> fromExprNumericValue(
		query::Context&                          ctx,
		pst::Access<pst::expr::ExprNumericValue> literal_expr,
		const dia::StablePosition&               position,
		const LiteralSign                        sign
	) {
		auto value          = literal_expr->getValue().value.strView();
		auto type_specifier = literal_expr->getTypeSpecifier();

		int base = 10;
		if (value.starts_with("0b") || value.starts_with("0B")) {
			base = 2;
			value.remove_prefix(2);
		} else if (value.starts_with("0o") || value.starts_with("0O")) {
			base = 8;
			value.remove_prefix(2);
		} else if (value.starts_with("0x") || value.starts_with("0X")) {
			base = 16;
			value.remove_prefix(2);
		}

		// A negative literal is parsed with its sign, so that the smallest value of a signed type
		// fits into it, like `-128i8`.
		std::string negative_value;
		if (sign == LiteralSign::Negative) {
			CORE_ASSERT(
				canBeNegative(literal_expr), "A literal of an unsigned type is never negative."
			);
			negative_value = std::string("-").append(value);
			value          = negative_value;
		}

		switch (type_specifier) {
		case lang_def::NumericLiteralTypeSpecifier::NotATypeSpecifier: {
			bool is_float = value.find_first_of(".eE") != std::string_view::npos && base == 10;
			return is_float ? deduceFloatType(value, position, ctx)
			                : deduceIntegerType(value, base, position, ctx);
		}
		case lang_def::NumericLiteralTypeSpecifier::i8:
			return parseSignedInteger<std::int8_t>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i16:
			return parseSignedInteger<i16>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i32:
			return parseSignedInteger<i32>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i64:
			return parseSignedInteger<i64>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::u8:
			return parseUnsignedInteger<std::uint8_t>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::u16:
			return parseUnsignedInteger<u16>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::u32:
			return parseUnsignedInteger<u32>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::u64:
			return parseUnsignedInteger<u64>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::f32:
			return parseFloat<f32>(value, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::f64:
			return parseFloat<f64>(value, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::f16:
		case lang_def::NumericLiteralTypeSpecifier::f80:
		case lang_def::NumericLiteralTypeSpecifier::u128:
		case lang_def::NumericLiteralTypeSpecifier::i128:
		case lang_def::NumericLiteralTypeSpecifier::f128:
			throw base::NotYetImplemented(base::strConcat(
				"Unhandled type specifier in hout of expr: ",
				lang_def::numericLiteralTypeSpecifierToStr(type_specifier)
			));
		}
		return {};
	}
}
