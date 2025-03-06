
#include "parser.hpp"
#include <base/exceptions.hpp>
#include <base/type_traits.hpp>
#include <cstdlib>
#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>
#include <code_data/opcode_args.hpp>
#include <code_data/opcodes.hpp>
#include <lexer/lexer.hpp>
#include <limits>
#include <queue>
#include <sstream>
#include <token_file/file.hpp>
#include <lexer/classifications.hpp>
#include <base/optional.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <stdexcept>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/tpc.hpp>
#include <base/maps.hpp>
#include <type_traits>
#include <utility>
#include <variant>
#include <base/variant.hpp>
#include <base/string_id.hpp>
#include <expected>
#include <base/box.hpp>
#include "errors.hpp"
#include <token_parser_core/common_elements.hpp>
#include <lexer/token.hpp>

namespace vm::parser {
	class F8ParserState final: public tpc::ParserState {
	public:
		F8ParserState(tpc::TokenStream&& stream, dia::Logger& err):
			  tpc::ParserState(std::move(stream), err) {}

		tpc::GenericAutomatic<F8ParserState> parse() { return { *this }; }
	};

	namespace opargs_parsers {
		template<class T, class K>
		T parseInt(F8ParserState& state) {
			auto token = state.tokens().next();
			try {
				usize  pos    = 0;
				auto&& str    = token.getValue().str();
				T      result = std::stoi(str, &pos);
				// Check if the number was fully parsed
				if (pos == str.length()) return result;
			} catch (std::logic_error& e) {}

			state.log(makeBox<vm::parser::InvalidLiteral>(
				token.getPosition(),
				base::strConcat("Not a valid number for `", base::typeName<K>(), "`.")
			));
			return T{ 0 };
		}

		base::StrID parseStr(F8ParserState& state) {
			tpc::Identifier identifier;
			state.parse().one(&identifier);
			return identifier.value;
		}

		template<class T>
		concept IsOpCodeArg = std::is_constructible_v<vm::opargs::OpCodeArg, T>;

		template<IsOpCodeArg ArgType>
		auto parseArg(F8ParserState& state) -> ArgType;

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::ImmediateI64 {
			return { parseInt<i64, vm::opargs::ImmediateI64>(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::StackOffset {
			return { parseInt<i64, vm::opargs::StackOffset>(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::ArgsOffset {
			return { parseInt<i64, vm::opargs::ArgsOffset>(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Type {
			return { parseStr(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::FunctionName {
			return { parseStr(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Label {
			return { parseStr(state) };
		}

		std::vector<OpCodeArgAndPosition> parseOpCode0Args(F8ParserState&) { return {}; }

		template<IsOpCodeArg Arg0>
		std::vector<OpCodeArgAndPosition> parseOpCode1Args(F8ParserState& state) {
			auto pos0 = state.getPosition();
			auto arg0 = parseArg<Arg0>(state);
			return { { arg0, pos0 } };
		}

		template<IsOpCodeArg Arg0, IsOpCodeArg Arg1>
		std::vector<OpCodeArgAndPosition> parseOpCode2Args(F8ParserState& state) {
			auto pos0 = state.getPosition();
			auto arg0 = parseArg<Arg0>(state);
			state.parse().one(lang_def::Special::Comma);
			auto pos1 = state.getPosition();
			auto arg1 = parseArg<Arg1>(state);
			return { { arg0, pos0 }, { arg1, pos1 } };
		}

#define MAKE_LINK(opcode, func) std::make_pair(std::string(#opcode), func),

#define HANDLE_OPCODE_0ARGS(opcode)            MAKE_LINK(opcode, parseOpCode0Args)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) MAKE_LINK(opcode, parseOpCode1Args<arg0_type>)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	MAKE_LINK(opcode, parseOpCode2Args<arg0_type COMMA arg1_type>)

		const std::unordered_map OP_CODE_TO_ARGS_PARSER = {
#include <code_data/opcodes_list.hpp>
		};

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
#undef MAKE_LINK
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
						state.log(
							makeBox<vm::parser::ExpectedSemicolonAfterError>(state.getPosition(-1))
						);
						logged = true;
						continue;
					}
					return out;
				} else if (!logged) {
					state.log(makeBox<vm::parser::UnknownOpCodeError>(
						state.getPosition(-1), identifier1.value
					));
					logged = true;
				}
			} else {
				if (!logged) {
					state.log(makeBox<tpc::NoIdentifierError>(state.getPosition()));
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
			auto opt_opcode = OpCode::parse(state).toOptBox();

			if (opt_opcode) {
				auto opcode = std::move(opt_opcode.value());
				if (opcode->opcode_name == base::StrID("label")) {
					auto arg = opcode->args[0];
					CORE_ASSERT(
						std::holds_alternative<vm::opargs::Label>(arg.arg),
						"Something went wrong during label parsing."
					);
					auto label_name = std::get<vm::opargs::Label>(arg.arg).label_name;

					if (out->label_position.contains(label_name)) {
						auto msg
							= makeBox<vm::parser::InvalidLabel>(arg.position, "Repeated label.");
						for (auto&& lbl: labels)
							if (lbl.first == label_name)
								msg->addNote(makeBox<vm::parser::RepeatedLabelNote>(lbl.second));
						state.log(std::move(msg));
					} else {
						out->label_position.put(label_name, out->opcodes.size());
						labels.emplace_back(label_name, arg.position);
					}
				} else {
					out->opcodes.emplace_back(std::move(opcode));
				}
			}
		}

		return out;
	}

	MBox<Func> Func::parse(F8ParserState& state) {
		auto out = makeBox<Func>(state.getPosition());

		state.parse().all(lang_def::Keyword::BCFunction, &out->name);

		if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
			return nullptr;
		}

		state.goDown();

		while (state.ctokens().peek().isKeyword()) {
			auto next = state.tokens().next();

			switch (next.asKeyword()) {
			case lang_def::Keyword::BCArgSize: {
				state.parse().one(lang_def::NamedOperator::Colon);
				if (out->arg_size != SIZE_T_MAX)
					state.err.failAndLog(state.getPosition(), "arg_size duplicate");
				auto value = state.tokens().next();

				if (!value.isNumLiteral())
					state.err.failAndLog(
						state.getPosition(), "arg_size argument is not num-literal"
					);
				try {
					out->arg_size = strIDToNum<usize>(value.getValue());
				} catch (std::logic_error& e) {
					state.err.failAndLog(
						state.getPosition(), "arg_size argument is not num-literal"
					);
				}
				state.parse().one(lang_def::Special::Semicolon);
				break;
			}
			case lang_def::Keyword::BCNextArgSize: {
				state.parse().one(lang_def::NamedOperator::Colon);
				if (out->next_arg_size != SIZE_T_MAX) {
					state.err.failAndLog(
						state.ctokens().peek().getPosition(), "next_arg_size duplicate"
					);
				}
				auto value = state.tokens().next();

				if (!value.isNumLiteral()) {
					state.err.failAndLog(
						state.ctokens().peek().getPosition(),
						"next_arg_size argument is not num-literal"
					);
				}
				try {
					out->next_arg_size = strIDToNum<usize>(value.getValue());
				} catch (std::logic_error& e) {
					state.err.failAndLog(
						state.ctokens().peek().getPosition(),
						"next_arg_size argument is not num-literal"
					);
				}
				state.parse().one(lang_def::Special::Semicolon);
				break;
			}

			case lang_def::Keyword::BCLocalSize: {
				state.parse().one(lang_def::NamedOperator::Colon);
				if (out->local_size != SIZE_T_MAX)
					state.err.failAndLog(state.getPosition(), "local_size duplicate");
				auto value = state.tokens().next();
				if (!value.isNumLiteral())
					state.err.failAndLog(
						state.getPosition(), "local_size argument is not num-literal"
					);
				try {
					out->local_size = strIDToNum<usize>(value.getValue());
				} catch (std::logic_error& e) {
					state.err.failAndLog(
						state.getPosition(), "local_size argument is not num-literal"
					);
				}

				state.parse().one(lang_def::Special::Semicolon);
				break;
			}

			case lang_def::Keyword::BCRetSize: {
				state.parse().one(lang_def::NamedOperator::Colon);
				if (out->ret_size != SIZE_T_MAX)
					state.err.failAndLog(state.getPosition(), "ret_size duplicate");
				auto value = state.tokens().next();
				if (!value.isNumLiteral())
					state.err.failAndLog(
						state.getPosition(), "ret_size argument is not num-literal"
					);
				try {
					out->ret_size = strIDToNum<usize>(value.getValue());
				} catch (std::logic_error& e) {
					state.err.failAndLog(
						state.getPosition(), "ret_size argument is not num-literal"
					);
				}

				state.parse().one(lang_def::Special::Semicolon);
				break;
			}

			case lang_def::Keyword::BCDefine: {
				throw base::NotYetImplemented("BCDefine");
			}

			case lang_def::Keyword::BCCode: {
				state.parse().one(lang_def::NamedOperator::Colon);
				if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly))
					state.err.failAndLog(state.getPosition(), "no {} on code:");

				state.goDown();
				state.parse().one(&out->code);
				state.goUpAndSkip();
				break;
			}

			default:
				state.err.failAndLog(state.getPosition(), "bad keyword in function");
			}
		}

		if (state.notEmpty()) {
			state.err.failAndLog(state.getPosition(-1), "Unexpected function content!");

			state.goUpAndSkip();
			return nullptr;
		}

		state.goUpAndSkip();

		if (out->local_size == SIZE_T_MAX) state.fail(0, "Local size not set");
		if (out->arg_size == SIZE_T_MAX) state.fail(0, "Arg size not set");
		if (out->next_arg_size == SIZE_T_MAX) state.fail(0, "Next arg size not set");
		if (out->ret_size == SIZE_T_MAX) state.fail(0, "Ret size not set");

		return out;
	}

	MBox<Type> Type::parse(F8ParserState& state) {
		state.parse().one(lang_def::Keyword::BCType);


		/// @TODO: implement keywordToNumLiteral
		auto type = state.tokens().next().asKeyword();
		state.parse().one(lang_def::NamedOperator::Colon);

		auto        out  = makeBox<Type>(state.getPosition());
		base::StrID name = state.tokens().next().getValue();

		switch (type) {
		case lang_def::Keyword::BCPrimitive: {
			lexer::Token value = state.tokens().next();
			if (!value.isNumLiteral()) {
				state.err.failAndLog(state.getPosition(), "expected number");
			} else {
				out->datatype
					= PrimitiveType{ .name = name,
					                 .size = static_cast<usize>(strIDToNum(value.getValue())) };
			}
			break;
		}
		case lang_def::Keyword::BCPointer: {
			auto pointered_type = state.tokens().next();
			if (!pointered_type.isIdentifier())
				state.err.failAndLog(state.getPosition(), "expected identifier");
			else
				out->datatype = PointerType{ .name = name, .inner = pointered_type.getValue() };
			break;
		}
		case lang_def::Keyword::BCStaticTable: {
			auto type_name = state.tokens().next();
			if (!type_name.isIdentifier()) {
				state.err.failAndLog(state.getPosition(), "expected identifier");
			} else {
				auto size = state.tokens().next();
				if (!size.isNumLiteral()) {
					state.err.failAndLog(state.getPosition(), "expected number");
				} else {
					out->datatype
						= StaticTableType{ .name  = name,
						                   .inner = type_name.getValue(),
						                   .table_size
						                   = static_cast<usize>(strIDToNum(size.getValue())) };
				}
			}
			break;
		}
		case lang_def::Keyword::BCDynamicTable: {
			const lexer::Token& type_name = state.tokens().next();
			if (!type_name.isIdentifier())
				state.err.failAndLog(state.getPosition(), "expected identifier");
			else
				out->datatype = DynamicTableType{ .name = name, .inner = type_name.getValue() };
			break;
		}
		case lang_def::Keyword::BCData: {
			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
				return nullptr;
			}

			state.goDown();
			std::vector<Field> fields;
			while (state.notEmpty()) {
				tpc::Identifier field_name;
				tpc::Identifier field_type;
				state.parse().all(&field_name, lang_def::NamedOperator::Colon, &field_type);
				fields.emplace_back(Field{ .name = field_name.value, .type = field_type.value });

				if (state.empty()) break;

				if (state.ctokens().peek().is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.err.failAndLog(state.getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			out->datatype = DataType{ name, fields };
			break;
		}
		case lang_def::Keyword::BCVariant: {
			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
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
					state.err.failAndLog(state.getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			out->datatype = VariantType{ .name = name, .variant_alternatives = alternatives };
			break;
		}
		case lang_def::Keyword::BCFunType: {
			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
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
					state.err.failAndLog(state.getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			tpc::Identifier result;
			state.parse().one(&result);
			out->datatype = FunctionType{ .name = name, .parameters = arguments, .result = result };
			break;
		}
		default: {
			state.err.failAndLog(state.getPosition(), "expected variant of type");
			break;
		}
		}

		return out;
	}

	MBox<ParsedFile> ParsedFile::parse(F8ParserState& state) {
		auto out = makeBox<ParsedFile>(state.getPosition());
		while (state.notEmpty()) {
			if (state[0].is(lang_def::Keyword::BCType)) {
				auto type = Type::parse(state).toOptBox();
				if (type) out->types.emplace_back(std::move(*type));
			} else if (state[0].is(lang_def::Keyword::BCFunction)) {
				auto func = Func::parse(state).toOptBox();
				if (func) out->functions.emplace_back(std::move(*func));
			} else {
				state.fail(0, "Unexpected keyword");
				break;
			}
		}

		return out;
	}

	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		lexer::init();
		tpc::init();
		lang_def::setKeywordMode(lang_def::KeywordMode::DuckBC);
		return lexer::tokenizeFile(path);
	}

	MBox<ParsedFile> parseFile(Ref<tokenizer::TokenFile> file, dia::Logger& log) {
		const lexer::TokenData& td = file->getTokenData();

		F8ParserState state(
			tpc::TokenStream(td.tokens, td.bof_sentinel, td.eof_sentinel, 0, td.tokens.size()), log
		);

		return ParsedFile::parse(state);
	}

	void defineTypes(Ref<ParsedProgram> program, dia::Logger& log) {
		base::Map<base::StrID, vm::TypeRef> type_map;

		std::deque<Ref<Type>> good_types;

		for (auto& type: program->types) {
			base::StrID name = VISIT(type->datatype, value, return value.name);

			if (type_map.contains(name)) {
				auto msg = makeBox<vm::parser::DuplicatedTypeError>(*type->position);

				auto duplicated_types
					= program->types | std::views::filter([&](auto&& duplicated_type) {
						  base::StrID other_name
							  = VISIT(duplicated_type->datatype, value, return value.name);
						  return name == other_name && (&*duplicated_type != &*type);
					  });

				for (auto& duplicated_type: duplicated_types)
					msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(*duplicated_type->position)
					);

				log.log(std::move(msg));
			} else {
				vm::Type typ      = vm::Type::declareType(name);
				auto     type_ref = program->type_metadata->addType(std::move(typ));
				type_map.put(name, type_ref);
				good_types.push_back(type.refMut());
			}
		}

		for (auto& type: good_types) {
			variant_match(type->datatype) {
				variant_case(PrimitiveType, data) {
					type_map[data.name]->definePrimitive(data.size);
				}
				variant_case(PointerType, data) {
					type_map[data.name]->definePointer(type_map[data.inner]);
				}
				variant_case(StaticTableType, data) {
					type_map[data.name]->defineStaticTable(type_map[data.inner], data.table_size);
				}
				variant_case(DynamicTableType, data) {
					type_map[data.name]->defineDynamicTable(type_map[data.inner]);
				}
				variant_case(DataType, data) {
					std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, type_map[field.type]);
					type_map[data.name]->defineData(fields);
				}
				variant_case(VariantType, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.variant_alternatives.size());
					for (auto& variant: data.variant_alternatives)
						variants.emplace_back(type_map[variant]);
					type_map[data.name]->defineVariant(variants);
				}
				variant_case(FunctionType, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters) parameters.emplace_back(type_map[param]);
					type_map[data.name]->defineFunction(parameters, type_map[data.result]);
				}
				variant_default { CORE_PANIC("bad type"); }
			}
		}

		program->type_metadata->finalize();
	}

	std::expected<ParsedProgram, std::string> assemble(const std::vector<fs::FilePath>& files) {
		auto          log = dia::Logger();
		ParsedProgram parsed_program;

		for (const auto& file: files) {
			parsed_program.token_files.push_back(tokenizeFile(file));
			auto parsed = parseFile(parsed_program.token_files.back().refMut(), log).toOptBox();

			parsed_program.files_src_pos.push_back(std::move(parsed.value()->position));

			for (auto& func: parsed.value()->functions) {
				auto func_name = func->name.value;
				if (parsed_program.name_to_func.contains(func_name)) {
					auto msg
						= makeBox<vm::parser::DuplicateFunctionDeclarationError>(*func->position);
					auto dup_func = parsed_program.name_to_func.atMaybe(func_name);
					msg->addNote(makeBox<vm::parser::DuplicatedFunctionDeclarationNote>(
						*dup_func.value()->position
					));
					log.log(std::move(msg));
				}

				parsed_program.functions.push_back(std::move(func));
				parsed_program.name_to_func.put(
					func_name, parsed_program.functions.back().refMut()
				);
			}

			for (auto& type: parsed.value()->types) parsed_program.types.push_back(std::move(type));

			if (!parsed || log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			}
		}

		defineTypes(&parsed_program, log);
		if (log.bad()) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}

		return parsed_program;
	}
}
