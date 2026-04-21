#include "numeric_literals.hpp"

#include "errors.hpp"

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <frontend/pst_parser/stable_position.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <charconv>
#include <string_view>
#include <system_error>

namespace compiler::helios::code {
	namespace {
		/**
		 * @brief Checks the given `from_chars_result` and logs compiler errors in case of errors.
		 * @return True `from_chars` succeeded, false if an error occurred, an appropriate compiler
		 * error is logged.
		 */
		bool handleFromCharsFailure(
			const std::from_chars_result& result,
			std::string_view              value,
			const pst::StablePosition&    position,
			query::Context&               ctx
		) {
			if (result.ec != std::errc()) {
				if (result.ec
				    == std::errc::invalid_argument) {  // Not a number at all. This will be returned
					                                   // when trying to parse "abc".
					ctx.logInt(
						makeBox<InvalidNumericLiteralError>(position.getActiveSourcePosition(ctx))
					);
				} else if (result.ec == std::errc::result_out_of_range) {
					ctx.logInt(
						makeBox<NumericLiteralTooLargeError>(position.getActiveSourcePosition(ctx))
					);
				}
				return false;
			}

			// Checks if the whole number was parsed. For example for a literal like this "123abc"
			// `from_chars` won't return the `std::errc::invalid_argument`, but return a parsed
			// "123" literal and stop on the first non numeric char. Here we check that the whole
			// string was parsed.
			if (result.ptr != value.data() + value.size()) {
				ctx.logInt(makeBox<InvalidNumericLiteralError>(position.getActiveSourcePosition(ctx)
				));
				return false;
			}
			return true;
		}

		template<typename TargetInt>
		base::Optional<numeric_value::NumericValue> parseSignedInteger(
			std::string_view value, int base, const pst::StablePosition& position, query::Context& ctx
		) {
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};

			if (!base::fitsIn<TargetInt>(parsed_value)) {
				ctx.logInt(makeBox<LiteralDoesNotFitError>(
					position.getActiveSourcePosition(ctx), "signed integer type"
				));
				return {};
			}

			return numeric_value::NumericValue(static_cast<TargetInt>(parsed_value));
		}

		template<typename TargetUInt>
		base::Optional<numeric_value::NumericValue> parseUnsignedInteger(
			std::string_view value, int base, const pst::StablePosition& position, query::Context& ctx
		) {
			u64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			if (!base::fitsIn<TargetUInt>(parsed_value)) {
				ctx.logInt(makeBox<LiteralDoesNotFitError>(
					position.getActiveSourcePosition(ctx), "unsigned integer type"
				));
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetUInt>(parsed_value));
		}

		template<typename TargetFloat>
		base::Optional<numeric_value::NumericValue> parseFloat(
			std::string_view value, const pst::StablePosition& position, query::Context& ctx
		) {
			f64  parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);

			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			// @note: No need to check with fitsIn, since casting to smaller types is always okay.
			return numeric_value::NumericValue(static_cast<TargetFloat>(parsed_value));
		}

		base::Optional<numeric_value::NumericValue> deduceIntegerType(
			std::string_view value, int base, const pst::StablePosition& position, query::Context& ctx
		) {
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);
			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			return numeric_value::NumericValue::createMinimized(parsed_value);
		}

		base::Optional<numeric_value::NumericValue> deduceFloatType(
			std::string_view value, const pst::StablePosition& position, query::Context& ctx
		) {
			f64  parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
			if (!handleFromCharsFailure(result, value, position, ctx)) return {};
			return numeric_value::NumericValue::createMinimized(parsed_value);
		}
	}

	base::Optional<compiler::numeric_value::NumericValue> fromExprNumericValue(
		query::Context& ctx, pst::AccessLocked<pst::expr::ExprNumericValue> literal_expr_locked
	) {
		auto literal_expr = literal_expr_locked.unlock(ctx);
		auto value        = literal_expr->getValue().value.strView();
		auto type_specifier_strid
			= literal_expr->getValue().type_specifier.copyValueOr(base::StrID(""));
		auto type_specifier = lang_def::strAsNumericLiteralTypeSpecifier(type_specifier_strid);
		auto position       = literal_expr->getStablePosition();

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
