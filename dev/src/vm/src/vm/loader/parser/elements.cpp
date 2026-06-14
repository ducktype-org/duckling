#include "elements.hpp"

#include <diagnostic_interactive/placeholder.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/floats.hpp>

#include <diagnostic/source_position.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/token_stream.hpp>
#include <token_source/source.hpp>

#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/const_value_visitor.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <cstring>

namespace vm::loader::parser {
	namespace opargs_parsers {
		namespace detail {
			/**
			 * @brief "Packs" bits of value `V` to 64-bit type`T`.
			 * 32-bit values are zero-extended to 64-bits and bit casted to T
			 * 64-bit values are just bit casted to T.
			 */
			template<typename T, typename V>
			T packValue(V value) {
				static_assert(sizeof(T) == 8, "T must be a 64-bit type");
				if constexpr (sizeof(V) == 8) {
					return std::bit_cast<T>(value);
				} else if constexpr (sizeof(V) == 4) {
					auto bits = std::bit_cast<u32>(value);
					return std::bit_cast<T>(static_cast<u64>(bits));
				}
			}

			/**
			 * @brief Checks if the value fits in bounds of the given signed type.
			 * Logs an error otherwise.
			 */
			template<typename SignedInt>
			bool checkSignedBoundsAndLog(
				F8ParserState& state, const lexer::Token& token, u64 raw_val, i32 sign
			) {
				if (sign == 1) {
					if (raw_val > static_cast<u64>(std::numeric_limits<SignedInt>::max())) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							base::strConcat(
								"Numeric literal overflows a ",
								base::toString(sizeof(SignedInt) * 8),
								"-bit signed integer."
							),
							token.getPosition()
						));
						return false;
					}
				} else if (raw_val
				           > (static_cast<u64>(std::numeric_limits<SignedInt>::max()) + 1ULL)) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Numeric literal underflows a ",
							base::toString(sizeof(SignedInt) * 8),
							"-bit signed integer."
						),
						token.getPosition()
					));
					return false;
				}
				return true;
			}

			/**
			 * @brief Checks if the value fits in bounds of the given unsigned type.
			 * Logs an error otherwise.
			 */
			template<typename UnsignedInt>
			bool checkUnsignedBoundsAndLog(
				F8ParserState& state, const lexer::Token& token, u64 raw_val, i32 sign
			) {
				if (sign == -1) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Unsigned integer literal cannot be negative.", token.getPosition()
					));
					return false;
				}
				if (raw_val > std::numeric_limits<UnsignedInt>::max()) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Numeric literal overflows a ",
							base::toString(sizeof(UnsignedInt) * 8),
							"-bit unsigned integer."
						),
						token.getPosition()
					));
					return false;
				}
				return true;
			}
		}

		/**
		 * @brief Parses number literal and returns it's bits stored in type T. If it contains a
		 * type specifier like `i32`, `f64`, etc., it will adjust the parsing behavior accordingly.
		 * By default, it assumes 64-bit integer or f64 if it has a dot or an `e` (works for hex
		 * and binary too). T has to be type of size 64bits.
		 */
		template<class T>
		std::pair<T, usize> parseNumericLiteral(F8ParserState& state) {
			usize literal_length = 0;
			i32   sign           = 1;

			const auto& maybe_sign_token = state.tokens().peek();
			if (maybe_sign_token.isOperatorSymbol()) {
				if (maybe_sign_token.getValue().str() == "-") {
					sign = -1;
					state.tokens().next();
					literal_length++;
				} else if (maybe_sign_token.getValue().str() == "+") {
					sign = 1;
					state.tokens().next();
					literal_length++;
				}
			}

			const auto& token = state.tokens().peek();
			if (!token.isNumLiteralGroup()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected a numeric literal.", token.getPosition()
				));
				return { T{ 0 }, 0 };
			}
			state.tokens().next();
			literal_length += token.getValue().strView().length();

			std::string_view number;
			std::string_view suffix;

			const auto& sub_tokens = token.getRecursive();
			number                 = sub_tokens[0].getValue().strView();
			suffix = sub_tokens.size() > 1 ? sub_tokens[1].getValue().strView() : "";

			int base = 10;
			// stoull crashes when '0b'/'0o' is a part of the string, thus we remove it.
			if (number.starts_with("0b") || number.starts_with("0B")) {
				base = 2;
				number.remove_prefix(2);
			} else if (number.starts_with("0o") || number.starts_with("0O")) {
				base = 8;
				number.remove_prefix(2);
			} else if (number.starts_with("0x") || number.starts_with("0X")) {
				base = 16;
				number.remove_prefix(2);
			}

			T           result = 0;
			usize       pos    = 0;
			std::string str(number);
			try {
				if (!suffix.empty()) {  // Type specifier exists.
					if (suffix.starts_with("f") && base != 10) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Floating-point literals must be in decimal base "
							"for: ",
							token.getPosition()
						));
						return { T{ 0 }, 0 };
					}

					if (suffix == "f32") {
						f32 value = std::stof(str, &pos) * static_cast<f32>(sign);
						result    = detail::packValue<T>(value);
					} else if (suffix == "f64") {
						f64 value = std::stod(str, &pos) * static_cast<f64>(sign);
						result    = detail::packValue<T>(value);
					} else if (suffix == "i32") {
						u64 raw_val = std::stoull(str, &pos, base);
						if (detail::checkSignedBoundsAndLog<i32>(state, token, raw_val, sign)) {
							i32 value = static_cast<i32>(raw_val) * sign;
							result    = detail::packValue<T>(value);
						}
					} else if (suffix == "i64") {
						u64 raw_val = std::stoull(str, &pos, base);
						if (detail::checkSignedBoundsAndLog<i64>(state, token, raw_val, sign)) {
							i64 value = static_cast<i64>(raw_val) * sign;
							result    = detail::packValue<T>(value);
						}
					} else if (suffix == "u32") {
						u64 raw_val = std::stoull(str, &pos, base);
						if (detail::checkUnsignedBoundsAndLog<u32>(state, token, raw_val, sign)) {
							u32 value = static_cast<u32>(raw_val);
							result    = detail::packValue<T>(value);
						}
					} else if (suffix == "u64") {
						u64 raw_val = std::stoull(str, &pos, base);
						if (detail::checkUnsignedBoundsAndLog<u64>(state, token, raw_val, sign))
							result = detail::packValue<T>(raw_val);
					} else {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							base::strConcat(
								"Unknown type specifier `",
								suffix,
								"` for `",
								base::typeName<vm::opargs::Immediate>(),
								"`."
							),
							token.getPosition()
						));
						return { T{ 0 }, 0 };
					}
				} else {
					// By default, we assume 64-bit integer or a f64 if it has a dot.
					if (str.find_first_of(".eE") != std::string::npos && base == 10) {
						f64 value = std::stod(str, &pos) * static_cast<f64>(sign);
						result    = detail::packValue<T>(value);
					} else {
						u64 raw_val = std::stoull(str, &pos, base);
						i64 value   = static_cast<i64>(raw_val) * sign;
						result      = detail::packValue<T>(value);
					}
				}

				if (pos == str.length())
					return std::make_pair(result, literal_length);
				else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Number not read fully for `",
							base::typeName<vm::opargs::Immediate>(),
							"`."
						),
						token.getPosition()
					));
					return { T{ 0 }, 0 };
				}
			} catch (std::logic_error&) {}

			state.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"Not a valid number for `", base::typeName<vm::opargs::Immediate>(), "`."
				),
				token.getPosition()
			));
			return { T{ 0 }, 0 };
		}

		std::pair<std::array<std::byte, 8>, Bits> parseHexLiteral(F8ParserState& state) {
			const auto& token = state.tokens().peek();

			if (!token.isNumLiteralGroup()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected a hex numeric literal.", token.getPosition()
				));
				return { {}, Bits(0) };
			}
			state.tokens().next();

			std::string_view raw = token.getValue().strView();

			// -----------------------------
			// 1. Validate prefix
			// -----------------------------
			if (!raw.starts_with("0x") && !raw.starts_with("0X")) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Hex literal must start with 0x.", token.getPosition()
				));
				return { {}, Bits(0) };
			}

			raw.remove_prefix(2);

			// -----------------------------
			// 2. Parse via std::stoull
			// -----------------------------
			std::string str(raw);
			std::size_t pos = 0;

			u64 value = 0;

			try {
				value = std::stoull(str, &pos, 16);

				if (pos != str.size()) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Hex literal not fully consumed.", token.getPosition()
					));
					return { {}, Bits(0) };
				}
			} catch (...) {
				state.logInt(
					makeBox<dia_int::PlaceholderError>("Invalid hex literal.", token.getPosition())
				);
				return { {}, Bits(0) };
			}

			usize                    bit_length = str.size() * 4;
			std::array<std::byte, 8> bytes{};
			std::memcpy(bytes.data(), &value, 8);

			return { bytes, Bits(bit_length) };
		}

		base::StrID parseStr(F8ParserState& state) {
			tpc::Identifier identifier;
			state.parse().one(&identifier);
			return identifier.value;
		}

		template<class T>
		concept IsOpCodeArg = std::is_constructible_v<vm::opargs::OpCodeArg, T>;

		/**
		 * @brief Parses opcode argument. Template specializations change only the return type.
		 */
		template<IsOpCodeArg ArgType>
		auto parseArg(F8ParserState& state) -> ArgType;

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Immediate {
			auto pos            = state.getPosition();
			auto parsed_literal = parseNumericLiteral<u64>(state);
			auto value          = parsed_literal.first;
			auto arg            = opargs::Immediate{ value };
			arg.bytecode_pos    = dia::SourcePosition(
                pos.getLocation(), pos.getStart(), pos.getStart() + parsed_literal.second
            );
			return arg;
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Field {
			tpc::Identifier type_name, field_name;
			state.parse().all(&type_name, lang_def::NamedOperator::Period, &field_name);
			auto field         = vm::opargs::Field{ type_name, field_name };
			field.bytecode_pos = dia::SourcePosition(
				type_name.position.getLocation(),
				type_name.position.getStart(),
				field_name.position.getEnd()
			);
			return field;
		}

#define HANDLE_STR_ARG(TYPE)                                                        \
	template<>                                                                      \
	auto parseArg(F8ParserState& state) -> vm::opargs::TYPE {                       \
		auto pos         = state.getPosition();                                     \
		auto value       = parseStr(state);                                         \
		auto arg         = vm::opargs::TYPE{ value };                               \
		arg.bytecode_pos = dia::SourcePosition(                                     \
			pos.getLocation(), pos.getStart(), pos.getStart() + value.view().size() \
		);                                                                          \
		return { arg };                                                             \
	}

		FOR_EACH(
			HANDLE_STR_ARG,
			Type,
			FunctionName,
			BuiltinFunctionName,
			ExtCFunctionName,
			MethodName,
			Label,
			VM_OPARG_PLACE_TYPES
		)

#undef HANDLE_STR_ARG

		// Dummy parameter to help with leading commas from macros.
		template<typename Dummy, typename ArgsHead = void, typename... ArgsTail>
		std::vector<opargs::OpCodeArg> parseOpCodeArgs(F8ParserState& state) {
			// Separate case for no (non-dummy) args passed (first arg defaulted)
			// to avoid a trailing comma.
			if constexpr (std::same_as<ArgsHead, void>) {
				return {};
			} else {
				return { parseArg<ArgsHead>(state),
					     (state.parse().one(lang_def::Special::Comma),
					      parseArg<ArgsTail>(state))... };
			}
		}

		const std::unordered_map OP_CODE_TO_ARGS_PARSER = {
#define ARG_TYPE(type, name) , type
#define HANDLE_INSTR_ARGS(NAME, ...) \
	std::make_pair(std::string{ #NAME }, parseOpCodeArgs<void FOR_EACH(ARG_TYPE EXPAND, __VA_ARGS__)>),

#include <vm/bytecode/instruction_definitions.hpp>
#undef HANDLE_INSTR_ARGS
#undef ARG_TYPE
		};
	}

#define ERROR_CHECK() \
	if (state.int_err->hasErrors()) return {};
#define PARSE_ONE_CHECK(VALUE) \
	state.parse().one(VALUE);  \
	ERROR_CHECK()

	namespace {
		base::Optional<Box<code::ConstantBase>> parseConstantValue(F8ParserState& state) {
			using namespace vm::code;

			if (state[0].isNumLiteralGroup()) {
				auto parsed = opargs_parsers::parseHexLiteral(state);
				ERROR_CHECK();
				ConstantImmediate immediate;
				immediate.size = base::bits2bytes(parsed.second);
				std::memcpy(immediate.content.data(), &parsed.first, 8);
				return makeBox<ConstantImmediate>(std::move(immediate));
			}

			if (state[0].isKeyword()) {
				auto kw = state[0].asKeyword();
				state.tokens().next();

				switch (kw) {
				case lang_def::Keyword::BCClass: {
					if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Expected `{` after `class`.", state.getPosition()
						));
						return {};
					}

					state.goDown();
					auto struct_val = makeBox<ConstantClass>();
					while (state.notEmpty()) {
						tpc::Identifier field_name;
						PARSE_ONE_CHECK(&field_name);
						PARSE_ONE_CHECK(lang_def::NamedOperator::Colon);

						auto field_value = parseConstantValue(state);
						if (!field_value) return {};
						struct_val->fields.emplace_back(
							field_name.value, std::move(field_value.value())
						);

						if (state.empty()) break;
						PARSE_ONE_CHECK(lang_def::Special::Comma);
					}
					state.goUpAndSkip();
					return std::move(struct_val);
				}
				case lang_def::Keyword::BCFixedSizeTable: {
					if (!state[0].isBracketGroup(lexer::Token::BracketType::Square)) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Expected `[` after `fixed_size_table`.", state.getPosition()
						));
						return {};
					}

					state.goDown();
					auto array_val = makeBox<ConstantFixedSizeTable>();
					while (state.notEmpty()) {
						auto element = parseConstantValue(state);
						if (!element) return {};

						array_val->elements.push_back(std::move(element.value()));
						if (state.empty()) break;
						PARSE_ONE_CHECK(lang_def::Special::Comma);
					}
					state.goUpAndSkip();

					return std::move(array_val);
				}
				default:
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Unexpected keyword in constant value.", state.getPosition()
					));
					return {};
				}
			}
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected a constant value here.", state.getPosition()
			));
			return {};
		}
	}  // namespace

	Box<GlobalData> GlobalData::parse(F8ParserState& state) {
		using namespace vm::code;
		auto out = makeBox<GlobalData>(state.getPosition());

		state.parse().one(lang_def::Keyword::BCGlobalData);

		state.parse().one(&out->name);
		state.parse().one(&out->type);

		if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected `{` after here.", state.getPosition(-1)
			));
			return out;
		}

		state.goDown();
		while (state.notEmpty()) {
			if (state[0].is(lang_def::Keyword::BCGlobalConstructor)) {
				tpc::Identifier value;
				state.parse().all(
					lang_def::Keyword::BCGlobalConstructor, lang_def::NamedOperator::Colon, &value
				);
				out->ctor_name = value;
			} else if (state[0].is(lang_def::Keyword::BCGlobalDestructor)) {
				tpc::Identifier value;
				state.parse().all(
					lang_def::Keyword::BCGlobalDestructor, lang_def::NamedOperator::Colon, &value
				);
				out->dtor_name = value;
			} else if (state[0].is(lang_def::Keyword::BCIsConstant)) {
				state.parse().one(lang_def::Keyword::BCIsConstant);
				state.parse().one(lang_def::NamedOperator::Colon);
				if (state[0].is(lang_def::Keyword::BCTrue)) {
					out->is_constant = true;
					state.tokens().next();
				} else if (state[0].is(lang_def::Keyword::BCFalse)) {
					out->is_constant = false;
					state.tokens().next();
				} else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected `true` or `false` after `is_constant:`.", state.getPosition()
					));
				}
			} else if (state[0].is(lang_def::Keyword::BCInitialValue)) {
				auto start_pos = state.getPosition().getStart();
				state.parse().one(lang_def::Keyword::BCInitialValue);
				state.parse().one(lang_def::NamedOperator::Colon);
				auto value_opt = parseConstantValue(state);
				auto end_pos   = state.getPosition(-1).getEnd();
				if (value_opt) {
					out->initial_value = ConstantValue(std::move(value_opt.value()));
					out->initial_value->bytecode_pos
						= dia::SourcePosition(out->position.getLocation(), start_pos, end_pos);
				}
			}

			if (state.empty()) break;
			if (state[0].is(lang_def::Special::Comma)) {
				state.parse().one(lang_def::Special::Comma);
			} else {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected comma or } after here.", state.getPosition()
				));
				state.tokens().skip();
			}
		}
		state.goUpAndSkip();

		auto end_position = state.getPosition(-1).getEnd();
		out->position     = dia::SourcePosition(
            out->position.getLocation(), out->position.getStart(), end_position
        );
		return out;
	}

	MBox<OpCode> OpCode::parse(F8ParserState& state) {
		tpc::Identifier identifier1;
		bool            logged = false;
		while (state.notEmpty()) {
			if (state[0].isIdentifier()) {
				state.parse().one(&identifier1);
				if (opargs_parsers::OP_CODE_TO_ARGS_PARSER.contains(identifier1.value.str())) {
					auto out         = makeBox<OpCode>(identifier1.position);
					out->opcode_name = identifier1.value;
					out->args
						= opargs_parsers::OP_CODE_TO_ARGS_PARSER.at(out->opcode_name.str())(state);
					if (!state.tryEat(lang_def::Special::Semicolon)) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Expected `;` after here.", state.getPosition(-1)
						));
						logged = true;
						continue;
					}

					// Update end to contains args
					if (!out->args.empty()) {
						auto end = VISIT(out->args.back(), arg, return *arg.bytecode_pos).getEnd();
						auto pos = out->position;

						out->position = { pos, end };
					}

					return out;
				} else if (!logged) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat("OpCode '", identifier1.value, "' does not exist."),
						state.getPosition(-1)
					));
					logged = true;
				}
			} else {
				if (!logged) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected an identifier here.", state.getPosition()
					));
					logged = true;
				}
				state.tokens().skip();
			}
		}
		return nullptr;
	}

	Box<ByteCode> ByteCode::parse(F8ParserState& state) {
		auto out = makeBox<ByteCode>(state.getPosition());

		std::vector<std::pair<base::StrID, dia::SourcePosition>> labels;

		while (state.notEmpty()) {
			if_opt_some(OpCode::parse(state).toOptBox(), opcode) {
				out->opcodes.emplace_back(std::move(opcode));
			}
		}

		return out;
	}

	MBox<Func> Func::parse(F8ParserState& state) {
		auto out = makeBox<Func>(state.getPosition());

		state.parse().all(lang_def::Keyword::BCFunction, &out->name);

		if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected `{` after here.", state.getPosition(-1)
			));
			return nullptr;
		}

		state.goDown();
		while (state.notEmpty()) {
			tpc::Identifier field_type;
			state.parse().one(&field_type);
			out->parameters.emplace_back(field_type);

			if (state.empty()) break;
			if (state[0].is(lang_def::Special::Comma)) {
				state.parse().one(lang_def::Special::Comma);
			} else {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected comma or `}` after here.", state.getPosition()
				));
				state.tokens().skip();
			}
		}
		state.goUpAndSkip();

		if (state[0].is(lang_def::NamedOperator::SingleArrow)) {
			state.parse().one(lang_def::NamedOperator::SingleArrow);
		} else {
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected `->` after function parameters.", state.getPosition(-1)
			));
			return nullptr;
		}

		if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected `{` after here `->`.", state.getPosition(-1)
			));
			return nullptr;
		}

		state.goDown();
		while (state.notEmpty()) {
			tpc::Identifier field_type;
			state.parse().one(&field_type);
			out->result_types.emplace_back(field_type);

			if (state.empty()) break;
			if (state[0].is(lang_def::Special::Comma)) {
				state.parse().one(lang_def::Special::Comma);
			} else {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected comma or `}` after here.", state.getPosition()
				));
				state.tokens().skip();
			}
		}
		state.goUpAndSkip();

		if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.logInt(makeBox<dia_int::PlaceholderError>(
				"Expected `{` after here.", state.getPosition(-1)
			));
			return nullptr;
		}

		state.goDown();
		state.parse().one(&out->code);
		state.goUpAndSkip();

		return out;
	}

	namespace {
		std::vector<code::Field> parseFields(F8ParserState& state) {
			std::vector<code::Field> out;

			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected `{` after here.", state.getPosition(-1)
				));
				return {};
			}

			state.goDown();
			while (state.notEmpty()) {
				tpc::Identifier field_name;
				tpc::Identifier field_type;
				state.parse().all(&field_name, lang_def::NamedOperator::Colon, &field_type);
				auto& field        = out.emplace_back(field_name.value, field_type.value);
				field.bytecode_pos = field_name.position;

				if (state.empty()) break;
				if (state[0].is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected comma or } after here.", state.getPosition()
					));
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();

			return out;
		}
	}

	MBox<Type> Type::parse(F8ParserState& state) {
		using namespace vm::code;

		state.parse().one(lang_def::Keyword::BCType);


		/// @TODO: implement keywordToNumLiteral
		auto type = state.tokens().next().asKeyword();
		state.parse().one(lang_def::NamedOperator::Colon);

		auto        out  = makeBox<Type>(state.getPosition());
		base::StrID name = state.tokens().next().getValue();

		switch (type) {
		case lang_def::Keyword::BCPrimitive: {
			lexer::Token value = state.tokens().next();
			if (!value.isNumLiteralGroup()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected a numeric literal after here.", state.getPosition()
				));
			} else {
				auto tp         = PrimitiveType{ name, Bytes{ strIDToNum(value.getValue()) } };
				tp.bytecode_pos = out->position;
				out->datatype   = tp;
			}
			break;
		}
		case lang_def::Keyword::BCPointer: {
			auto pointed_type = state.tokens().next();
			if (!pointed_type.isIdentifier())
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected an identifier after here.", state.getPosition()
				));
			else {
				auto tp         = PointerType{ name, pointed_type.getValue() };
				tp.bytecode_pos = out->position;
				out->datatype   = tp;
			}
			break;
		}
		case lang_def::Keyword::BCFixedSizeTable: {
			auto type_name = state.tokens().next();
			if (!type_name.isIdentifier()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected an identifier after here.", state.getPosition()
				));
			} else {
				auto size = state.tokens().next();
				if (!size.isNumLiteralGroup()) {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected a numeric literal after here.", state.getPosition()
					));
				} else {
					auto tp         = FixedSizeTableType{ name,
                                                  type_name.getValue(),
                                                  static_cast<usize>(strIDToNum(size.getValue())) };
					tp.bytecode_pos = out->position;
					out->datatype   = tp;
				}
			}
			break;
		}
		case lang_def::Keyword::BCDynamicTable: {
			const lexer::Token& type_name = state.tokens().next();
			if (!type_name.isIdentifier())
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected an identifier after here.", state.getPosition()
				));
			else {
				auto tp         = DynamicTableType{ name, type_name.getValue() };
				tp.bytecode_pos = out->position;
				out->datatype   = tp;
			}
			break;
		}
		case lang_def::Keyword::BCData: {
			auto tp         = DataType{ name, parseFields(state) };
			tp.bytecode_pos = out->position;
			out->datatype   = std::move(tp);
			break;
		}
		case lang_def::Keyword::BCVariant: {
			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected `{` after here.", state.getPosition(-1)
				));
				return nullptr;
			}

			state.goDown();
			std::vector<base::StrID> alternatives;
			while (state.notEmpty()) {
				tpc::Identifier field_type;
				state.parse().one(&field_type);
				alternatives.emplace_back(field_type.value);

				if (state.empty()) break;
				if (state[0].is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected comma or `}` after here.", state.getPosition()
					));
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			auto tp         = VariantType{ name, alternatives };
			tp.bytecode_pos = out->position;
			out->datatype   = std::move(tp);
			break;
		}
		case lang_def::Keyword::BCFunType: {
			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected `{` after here.", state.getPosition(-1)
				));
				return nullptr;
			}

			state.goDown();
			std::vector<base::StrID> arguments;
			while (state.notEmpty()) {
				tpc::Identifier field_type;
				state.parse().one(&field_type);
				arguments.emplace_back(field_type.value);

				if (state.empty()) break;
				if (state[0].is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected comma or `}` after here.", state.getPosition()
					));
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();

			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected `{` after here.", state.getPosition(-1)
				));
				return nullptr;
			}

			std::vector<base::StrID> returned;
			state.goDown();
			while (state.notEmpty()) {
				tpc::Identifier field_type;
				state.parse().one(&field_type);
				returned.emplace_back(field_type.value);

				if (state.empty()) break;
				if (state[0].is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected comma or `}` after here.", state.getPosition()
					));
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();

			auto tp         = FunctionType{ name, arguments, returned };
			tp.bytecode_pos = out->position;
			out->datatype   = std::move(tp);
			break;
		}
		case lang_def::Keyword::BCOpaque: {
			lexer::Token value = state.tokens().next();
			if (!value.isNumLiteralGroup()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected a numeric literal after here.", state.getPosition()
				));
			} else {
				auto tp         = OpaqueType{ name, Bytes{ strIDToNum(value.getValue()) } };
				tp.bytecode_pos = out->position;
				out->datatype   = tp;
			}
			break;
		}
		case lang_def::Keyword::BCClass:
		case lang_def::Keyword::BCInterface: {
			bool                        is_interface = type == lang_def::Keyword::BCInterface;
			bool                        is_abstract  = false;
			std::vector<Field>          fields;
			std::vector<base::StrID>    implements;
			std::vector<Field>          virtual_methods;
			std::vector<Field>          implementations;
			base::Optional<base::StrID> extends;

			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Expected `{` after here.", state.getPosition(-1)
				));
				return nullptr;
			}

			state.goDown();

			while (state[0].isKeyword()) {
				auto next = state.tokens().next();
				switch (next.asKeyword()) {
				case lang_def::Keyword::BCExtends: {
					if (is_interface) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Interfaces cannot extend classes.", state.getPosition()
						));
					} else {
						tpc::Identifier superclass;
						state.parse().all(
							lang_def::NamedOperator::Colon, &superclass, lang_def::Special::Semicolon
						);
						extends.emplace(superclass.value);
					}
					break;
				}
				case lang_def::Keyword::BCAbstract: {
					if (is_interface) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Interfaces cannot be abstract.", state.getPosition()
						));
					} else {
						state.parse().one(lang_def::NamedOperator::Colon);
						switch (state.tokens().next().asKeyword()) {
						case lang_def::Keyword::BCTrue:
							is_abstract = true;
							break;
						case lang_def::Keyword::BCFalse:
							is_abstract = false;
							break;
						default:
							state.logInt(makeBox<dia_int::PlaceholderError>(
								"Unexpected keyword.", state.getPosition()
							));
							break;
						}
						state.parse().one(lang_def::Special::Semicolon);
					}
					break;
				}
				case lang_def::Keyword::BCImplements: {
					state.parse().one(lang_def::NamedOperator::Colon);
					if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
						state.logInt(makeBox<dia_int::PlaceholderError>(
							"Expected `{` after here.", state.getPosition(-1)
						));
						return nullptr;
					}

					state.goDown();
					while (state.notEmpty()) {
						tpc::Identifier interface;
						state.parse().one(&interface);
						implements.push_back(interface.value);

						if (state.empty()) break;
						if (state[0].is(lang_def::Special::Comma)) {
							state.parse().one(lang_def::Special::Comma);
						} else {
							state.logInt(makeBox<dia_int::PlaceholderError>(
								"Expected comma or `}` after here.", state.getPosition()
							));
							state.tokens().skip();
						}
					}
					state.goUpAndSkip();
					state.parse().one(lang_def::Special::Semicolon);
					break;
				}
				case lang_def::Keyword::BCVirtualMethods: {
					state.parse().one(lang_def::NamedOperator::Colon);
					virtual_methods = parseFields(state);
					state.parse().one(lang_def::Special::Semicolon);
					break;
				}
				case lang_def::Keyword::BCMethodImplementations: {
					state.parse().one(lang_def::NamedOperator::Colon);
					implementations = parseFields(state);
					state.parse().one(lang_def::Special::Semicolon);
					break;
				}
				case lang_def::Keyword::BCFields: {
					state.parse().one(lang_def::NamedOperator::Colon);
					for (auto field: parseFields(state)) fields.push_back(field);
					state.parse().one(lang_def::Special::Semicolon);
					break;
				}
				default: {
					state.logInt(makeBox<dia_int::PlaceholderError>(
						"Unexpected keyword.", state.getPosition()
					));
					return nullptr;
				}
				}
			}

			if (state.notEmpty()) {
				state.logInt(makeBox<dia_int::PlaceholderError>(
					"Unexpected class/interface content.", state.getPosition(-1)
				));

				state.goUpAndSkip();
				return nullptr;
			}

			state.goUpAndSkip();

			auto tp = is_interface ? TypeOfData(InterfaceType(
										 name,
										 std::move(implements),
										 std::move(virtual_methods),
										 std::move(implementations)
									 ))
			                       : TypeOfData(ClassType(
										 name,
										 fields,
										 is_abstract,
										 extends,
										 std::move(implements),
										 std::move(virtual_methods),
										 std::move(implementations)
									 ));
			VISIT(tp, t, t.bytecode_pos = out->position);
			out->datatype = std::move(tp);
			break;
		}
		default: {
			state.logInt(makeBox<dia_int::PlaceholderError>("Expected a type.", state.getPosition())
			);
			break;
		}
		}

		return out;
	}

	MBox<ParsedFile> ParsedFile::parse(F8ParserState& state) {
		auto out = makeBox<ParsedFile>(state.getPosition().getSource()->getFile());
		while (state.notEmpty()) {
			if (state[0].is(lang_def::Keyword::BCType)) {
				auto type = Type::parse(state).toOptBox();
				if (type) out->types.emplace_back(std::move(*type));
			} else if (state[0].is(lang_def::Keyword::BCGlobalData)) {
				auto global_data = GlobalData::parse(state);
				out->global_data.emplace_back(std::move(global_data));
			} else if (state[0].is(lang_def::Keyword::BCFunction)) {
				auto func = Func::parse(state).toOptBox();
				if (func) out->functions.emplace_back(std::move(*func));
			} else {
				state.logInt(
					makeBox<dia_int::PlaceholderError>("Unexpected keyword.", state.getPosition())
				);
				break;
			}
		}

		return out;
	}

	tpc::GenericAutomatic<F8ParserState> F8ParserState::parse() { return { *this }; }

	void AsmElement::debugPrint(std::ostream& out) const { dprint(out); }

	void Type::dprint(std::ostream& out) const {
		out << "type: ";
		VARIANT_VISIT(datatype, VISIT_CASE(auto&, data, { vm::code::serializeType(data, out); }))
		out << "\n}";
	}

	void OpCode::dprint(std::ostream& out) const {
		out << "        " << opcode_name.view().stringView() << " ";
		for (auto& arg: args) {
			variant_match(arg) {
				variant_case(vm::opargs::Immediate, num_arg) { out << num_arg.value << " "; }

#define HANDLE_LOCAL(Type) \
	variant_case(vm::opargs::Type, local_type) { out << local_type.var_name.strView() << " "; }

				FOR_EACH(HANDLE_LOCAL, VM_OPARG_PLACE_TYPES);

#undef HANDLE_LOCAL

				variant_case(vm::opargs::Type, type_arg) {
					out << type_arg.type_name.strView() << " ";
				}
				variant_case(vm::opargs::FunctionName, function_name_arg) {
					out << function_name_arg.function_name.strView() << " ";
				}
				variant_case(vm::opargs::Label, label_arg) {
					out << label_arg.label_name.strView() << " ";
				}
			}
		}
		out << '\n';
	}

	void ByteCode::dprint(std::ostream& out) const {
		for (auto& opcode: opcodes) opcode->dprint(out);
	}

	void GlobalData::dprint(std::ostream& out) const {
		out << lang_def::keywordToStr(lang_def::Keyword::BCGlobalData).strView() << " ";
		out << name.value.strView() << " " << type.value.strView() << " {";
		if (is_constant) {
			out << "\n    " << lang_def::keywordToStr(lang_def::Keyword::BCIsConstant).strView()
				<< ": " << lang_def::keywordToStr(lang_def::Keyword::BCTrue).strView() << ",";
		}
		if (initial_value.has_value()) code::serializeConstValue(initial_value.value(), out);
		if (ctor_name.has_value()) {
			out << "\n    "
				<< lang_def::keywordToStr(lang_def::Keyword::BCGlobalConstructor).strView() << ": "
				<< ctor_name->value.strView() << ",";
		}
		if (dtor_name.has_value()) {
			out << "\n    "
				<< lang_def::keywordToStr(lang_def::Keyword::BCGlobalDestructor).strView() << ": "
				<< dtor_name->value.strView() << ",";
		}
		out << "\n};";
	}

	void Func::dprint(std::ostream& out) const {
		out << "function ";
		out << name.value.strView() << "{";
		bool first = true;
		for (const auto& param: parameters) {
			if (!first) out << ", ";
			out << param.value.strView();
			first = false;
		}
		out << "} -> { ";
		first = true;
		for (const auto& param: result_types) {
			if (!first) out << ", ";
			out << param.value.strView();
			first = false;
		}
		out << " } {\n";
		code->dprint(out);
		out << "}\n";
	}

	void ParsedFile::dprint(std::ostream& out) const {
		for (auto& type: types) {
			type->dprint(out);
			out << "\n";
		}

		for (auto& global: global_data) {
			global->dprint(out);
			out << "\n";
		}

		for (auto& func: functions) {
			func->dprint(out);
			out << "\n";
		}
	}

	ParsedFile::ParsedFile(fs::File source_file): source_file(std::move(source_file)) {}
}
