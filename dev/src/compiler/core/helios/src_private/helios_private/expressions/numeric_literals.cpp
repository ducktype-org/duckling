#include "numeric_literals.hpp"

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include "diagnostic/source_position.hpp"
#include "query_framework/context.hpp"
#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/query_impl.hpp>

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
		bool handleFromCharsResult(
			const std::from_chars_result& result,
			std::string_view              value,
			const dia::SourcePosition&    position,
			query::Context&               ctx
		) {
			if (result.ec != std::errc()) {
				if (result.ec
				    == std::errc::invalid_argument) {  // Not a number at all. This will be returned
					                                   // when trying to parse "abc".
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							position, "Invalid numeric literal"
						)
					);
				} else if (result.ec == std::errc::result_out_of_range) {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							position, "Numeric literal value is to large to be processed."
						)
					);
				}
				return false;
			}

			// Checks if the whole number was parsed. For example for a literal like this "123abc"
			// `from_chars` won't return the `std::errc::invalid_argument`, but return a parsed
			// "123" literal and stop on the first non numeric char. Here we check that the whole
			// string was parsed.
			if (result.ptr != value.data() + value.size()) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid numeric literal"
					)
				);
				return false;
			}
			return true;
		}

		template<typename TargetInt>
		base::Optional<numeric_value::NumericValue> parseSignedInteger(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsResult(result, value, position, ctx)) return {};

			if (!base::fitsIn<TargetInt>(parsed_value)) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Literal doesn't fit in the declared signed integer type"
					)
				);
				return {};
			}

			return numeric_value::NumericValue(static_cast<TargetInt>(parsed_value));
		}

		template<typename TargetUInt>
		base::Optional<numeric_value::NumericValue> parseUnsignedInteger(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			u64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (!handleFromCharsResult(result, value, position, ctx)) return {};
			if (!base::fitsIn<TargetUInt>(parsed_value)) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Literal doesn't fit in the declared unsigned integer type"
					)
				);
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetUInt>(parsed_value));
		}

		template<typename TargetFloat>
		base::Optional<numeric_value::NumericValue> parseFloat(
			std::string_view value, const dia::SourcePosition& position, query::Context& ctx
		) {
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);

			if (!handleFromCharsResult(result, value, position, ctx)) return {};
			// @note: No need to check with fitsIn, since casting to smaller types is always okay.
			return numeric_value::NumericValue(static_cast<TargetFloat>(parsed_value));
		}

		base::Optional<numeric_value::NumericValue> deduceIntegerType(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);
			if (!handleFromCharsResult(result, value, position, ctx)) return {};

			numeric_value::NumericValue numeric_result
				= numeric_value::NumericValue::createMinimized(parsed_value);

			// @TODO: #859 For now, until the cast instuction are added we cast all the deduced
			// types to i64 to avoid adding a type specifier to every numeric literal in the tests.
			return numeric_value::NumericValue{ std::visit(
				[&](auto&& val) { return static_cast<i64>(val); }, numeric_result.getStorage()
			) };
		}

		base::Optional<numeric_value::NumericValue> deduceFloatType(
			std::string_view value, const dia::SourcePosition& position, query::Context& ctx
		) {
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
			if (!handleFromCharsResult(result, value, position, ctx)) return {};
			return numeric_value::NumericValue::createMinimized(parsed_value);
		}

	}

	base::Optional<compiler::numeric_value::NumericValue> fromExprValue(
		query::Context& ctx, pst::AccessLocked<pst::expr::ExprValue> literal_expr_locked
	) {
		auto literal_expr = literal_expr_locked.unlock(ctx);
		auto value        = literal_expr->getValue().value.strView();
		auto type_specifier_strid
			= literal_expr->getValue().type_specifier.copyValueOr(base::StrID(""));
		auto type_specifier = lang_def::strAsNumericLiteralTypeSpecifier(type_specifier_strid);
		auto position       = literal_expr->getSourcePosition();

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
			bool is_float = value.find_first_of(".eE") != std::string_view::npos;
			return is_float ? deduceFloatType(value, position, ctx)
			                : deduceIntegerType(value, base, position, ctx);
		}
		case lang_def::NumericLiteralTypeSpecifier::i16:
			return parseSignedInteger<i16>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i32:
			return parseSignedInteger<i32>(value, base, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i64:
			return parseSignedInteger<i64>(value, base, position, ctx);
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
		case lang_def::NumericLiteralTypeSpecifier::f128:
			return parseFloat<f128>(value, position, ctx);
		case lang_def::NumericLiteralTypeSpecifier::i8:
		case lang_def::NumericLiteralTypeSpecifier::u8:
		case lang_def::NumericLiteralTypeSpecifier::f16:
		case lang_def::NumericLiteralTypeSpecifier::f80:
		case lang_def::NumericLiteralTypeSpecifier::u128:
		case lang_def::NumericLiteralTypeSpecifier::i128:
			throw base::NotYetImplemented(base::strConcat(
				"Unhandled type specifier in hout of expr: ",
				lang_def::numericLiteralTypeSpecifierToStr(type_specifier)
			));
		}
		return {};
	}
}
