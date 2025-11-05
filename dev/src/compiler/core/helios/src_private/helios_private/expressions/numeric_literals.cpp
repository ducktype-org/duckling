
/// NUMERIC LITERAL PARSING ///
#include "numeric_literals.hpp"

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/query_impl.hpp>

#include <charconv>
#include <limits>
#include <type_traits>

#define NDEBUG(CONTENT) std::cout << "[NUMERIC DEDUCTION]: " << CONTENT << '\n';

namespace compiler::helios::code {
	namespace {
		template<typename TargetType, typename SourceType>
		bool fitsIn(SourceType value) {
			if constexpr (std::is_signed_v<TargetType> == std::is_signed_v<SourceType>) {
				return value >= static_cast<SourceType>(std::numeric_limits<TargetType>::min())
				    && value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
			} else if constexpr (std::is_unsigned_v<SourceType> && std::is_unsigned_v<TargetType>) {
				return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
			} else {
				return value >= 0
				    && static_cast<SourceType>(value) <= std::numeric_limits<TargetType>::max();
			}
		}

		template<typename TargetInt>
		base::Optional<numeric_value::NumericValue> parseSignedInteger(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Parse signed int");
			// TODOP: i128 potentially?
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: signed integer"
					)
				);
				return {};
			}

			if (!fitsIn<TargetInt>(parsed_value)) {
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
			NDEBUG("Parse unsigned int");
			// TODOP: u128 potentially?
			u64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: unsigned integer"
					)
				);
				return {};
			}
			if (!fitsIn<TargetUInt>(parsed_value)) {
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
			NDEBUG("Parse float");
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);

			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: floating point"
					)
				);
				return {};
			}
			return numeric_value::NumericValue(static_cast<TargetFloat>(parsed_value));
		}

		base::Optional<numeric_value::NumericValue> deduceIntegerType(
			std::string_view value, int base, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Deduce integer type");
			i64  parsed_value = 0;
			auto result
				= std::from_chars(value.data(), value.data() + value.size(), parsed_value, base);
			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				NDEBUG("Deduce integer type from_chars error");
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: deduced integer type"
					)
				);
				return {};
			}

			// TODOP: issue, add support for i8 and i128 types
			numeric_value::NumericValue numeric_result;
			if (parsed_value <= std::numeric_limits<i16>::max()) {
				NDEBUG("i16");
				numeric_result = numeric_value::NumericValue{ static_cast<i16>(parsed_value) };
			} else if (parsed_value <= std::numeric_limits<i32>::max()) {
				NDEBUG("i32");
				numeric_result = numeric_value::NumericValue{ static_cast<i32>(parsed_value) };
			} else if (parsed_value <= std::numeric_limits<i64>::max()) {
				NDEBUG("i64");
				numeric_result = numeric_value::NumericValue{ static_cast<i64>(parsed_value) };
			} else {
				NDEBUG("Deduce integer type literal overflow");
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Integer literal overflow"
					)
				);
				return {};
			}

			// TODOP: Add issue number
			// @TODO: For now, until the cast instuction are added we cast all the deduced types to
			// i64 to avoid adding a type specifier to  every numeric literal in the tests.
			NDEBUG("CASTING TO I64");
			return numeric_value::NumericValue{ std::visit(
				[&](auto&& val) { return static_cast<i64>(val); }, numeric_result.getStorage()
			) };
		}

		base::Optional<numeric_value::NumericValue> deduceFloatType(
			std::string_view value, const dia::SourcePosition& position, query::Context& ctx
		) {
			NDEBUG("Deduce float type");
			f128 parsed_value = 0;
			auto result = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
			if (result.ec != std::errc()
			    || result.ptr != value.data() + value.size()) {  // Bad format. TODOP: Add comment.
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						position, "Invalid literal: deduced floating point type"
					)
				);
				return {};
			}

			// TODOP: Issue, add support for f16.
			if (static_cast<f128>(static_cast<f32>(parsed_value)) == parsed_value)
				return numeric_value::NumericValue{ static_cast<f32>(parsed_value) };
			else if (static_cast<f128>(static_cast<f64>(parsed_value)) == parsed_value)
				return numeric_value::NumericValue{ static_cast<f64>(parsed_value) };
			return numeric_value::NumericValue{ parsed_value };  // Full precision needed
		}

	}

	base::Optional<compiler::numeric_value::NumericValue> fromExprValue(
		query::Context& ctx, pst::AccessLocked<pst::expr::ExprValue> literal_expr_locked
	) {
		auto literal_expr = literal_expr_locked.unlock(ctx);

		NDEBUG("Visit expr value");
		NDEBUG("Called for:");
		literal_expr->dprint(std::cout);
		std::cout << "\n===========================\n";


		auto value = literal_expr->getValue().value.strView();
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
