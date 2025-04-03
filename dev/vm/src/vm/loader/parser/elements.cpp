#include "elements.hpp"

#include "errors.hpp"

#include <diagnostic/source_position.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/token_stream.hpp>

#include <base/for_each.hpp>
#include <base/optional.hpp>

#include <vm/code/opcode_args.hpp>

namespace vm::parser {
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
			} catch (std::logic_error&) {}

			state.log(makeBox<InvalidLiteral>(
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

#define HANDLE_OFFSET(Type)                                   \
	template<>                                                \
	auto parseArg(F8ParserState& state) -> vm::opargs::Type { \
		return { parseInt<i64, vm::opargs::Type>(state) };    \
	}

		FOR_EACH(HANDLE_OFFSET, Immediate, VM_OPARG_OFFSET_TYPES);

#undef HANDLE_OFFSET

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

		std::vector<opargs::OpCodeArg> parseOpCode0Args(F8ParserState&) { return {}; }

		template<IsOpCodeArg Arg0>
		std::vector<opargs::OpCodeArg> parseOpCode1Args(F8ParserState& state) {
			auto pos0         = state.getPosition();
			auto arg0         = parseArg<Arg0>(state);
			arg0.bytecode_pos = pos0;

			return { arg0 };
		}

		template<IsOpCodeArg Arg0, IsOpCodeArg Arg1>
		std::vector<opargs::OpCodeArg> parseOpCode2Args(F8ParserState& state) {
			auto pos0         = state.getPosition();
			auto arg0         = parseArg<Arg0>(state);
			arg0.bytecode_pos = pos0;
			state.parse().one(lang_def::Special::Comma);
			auto pos1         = state.getPosition();
			auto arg1         = parseArg<Arg1>(state);
			arg1.bytecode_pos = pos1;
			return { arg0, arg1 };
		}

#define MAKE_LINK(opcode, func) std::make_pair(std::string(#opcode), func),

#define HANDLE_OPCODE_0ARGS(opcode)            MAKE_LINK(opcode, parseOpCode0Args)
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) MAKE_LINK(opcode, parseOpCode1Args<arg0_type>)
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	MAKE_LINK(opcode, parseOpCode2Args<arg0_type COMMA arg1_type>)

		const std::unordered_map OP_CODE_TO_ARGS_PARSER = {
#include <vm/code/opcodes_list.hpp>

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

					// Update end to contains args
					if (!out->args.empty()) {
						auto end = VISIT(out->args.back(), arg, return *arg.bytecode_pos).getEnd();
						const auto& pos = *out->position;
						out->position
							= makeBox<dia::SourcePosition>(pos.getSource(), pos.getStart(), end);
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
		if (out->ret_size == SIZE_T_MAX) state.fail(0, "Ret size not set");

		return out;
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
			if (!value.isNumLiteral()) {
				state.err.failAndLog(state.getPosition(), "expected number");
			} else {
				auto tp = PrimitiveType{ name, static_cast<usize>(strIDToNum(value.getValue())) };
				tp.bytecode_pos = *out->position;
				out->datatype   = tp;
			}
			break;
		}
		case lang_def::Keyword::BCPointer: {
			auto pointed_type = state.tokens().next();
			if (!pointed_type.isIdentifier())
				state.err.failAndLog(state.getPosition(), "expected identifier");
			else {
				auto tp         = PointerType{ name, pointed_type.getValue() };
				tp.bytecode_pos = *out->position;
				out->datatype   = tp;
			}
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
					auto tp         = StaticTableType{ name,
                                               type_name.getValue(),
                                               static_cast<usize>(strIDToNum(size.getValue())) };
					tp.bytecode_pos = *out->position;
					out->datatype   = tp;
				}
			}
			break;
		}
		case lang_def::Keyword::BCDynamicTable: {
			const lexer::Token& type_name = state.tokens().next();
			if (!type_name.isIdentifier())
				state.err.failAndLog(state.getPosition(), "expected identifier");
			else {
				auto tp         = DynamicTableType{ name, type_name.getValue() };
				tp.bytecode_pos = *out->position;
				out->datatype   = tp;
			}
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
				fields.emplace_back(field_name.value, field_type.value);
				fields.back().bytecode_pos = field_name.position;


				if (state.empty()) break;

				if (state.ctokens().peek().is(lang_def::Special::Comma)) {
					state.parse().one(lang_def::Special::Comma);
				} else {
					state.err.failAndLog(state.getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			auto tp         = DataType{ name, fields };
			tp.bytecode_pos = *out->position;
			out->datatype   = tp;
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
			auto tp         = VariantType{ name, alternatives };
			tp.bytecode_pos = *out->position;
			out->datatype   = tp;
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
			auto tp         = FunctionType{ name, arguments, result };
			tp.bytecode_pos = *out->position;
			out->datatype   = tp;
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
		auto out = makeBox<ParsedFile>(state.getPosition().getSource()->getPath());
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

	tpc::GenericAutomatic<F8ParserState> F8ParserState::parse() { return { *this }; }

	void AsmElement::debugPrint(std::ostream& out) const { dprint(out); }

	void Type::dprint(std::ostream& out) const {
		out << "type: ";
		VARIANT_VISIT(datatype, VISIT_CASE(auto&, data, { data.dprint(out); }))
		out << "\n}";
	}

	void OpCode::dprint(std::ostream& out) const {
		out << "        " << opcode_name.view().stringView() << " ";
		for (auto& arg: args) {
			variant_match(arg) {
				variant_case(vm::opargs::Immediate, num_arg) { out << num_arg.value << " "; }

#define HANDLE_OFFSET(Type) \
	variant_case(vm::opargs::Type, offset_type) { out << offset_type.offset << " "; }

				FOR_EACH(HANDLE_OFFSET, VM_OPARG_OFFSET_TYPES);

#undef HANDLE_OFFSET

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
		out << "    code {\n";
		for (auto& opcode: opcodes) opcode->dprint(out);
		out << "    }\n";
	}

	void Func::dprint(std::ostream& out) const {
		out << "function {\n";
		out << "    name: " << name.value.strView() << "\n";
		out << "    arg_size: " << arg_size << "\n";
		out << "    local_size: " << local_size << "\n";
		out << "    ret_size: " << ret_size << "\n";
		code->dprint(out);
		out << "\n}";
	}

	void ParsedFile::dprint(std::ostream& out) const {
		for (auto& type: types) {
			type->dprint(out);
			out << "\n";
		}

		for (auto& func: functions) {
			func->dprint(out);
			out << "\n";
		}
	}

	ParsedFile::ParsedFile(fs::FilePath source_file): source_file(std::move(source_file)) {}
}
