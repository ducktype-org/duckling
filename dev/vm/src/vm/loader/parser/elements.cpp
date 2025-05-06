#include "elements.hpp"

#include "errors.hpp"

#include <diagnostic/source_position.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/token_stream.hpp>

#include <base/macros/for_each.hpp>
#include <base/optional.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

namespace vm::loader::parser {
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

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Immediate {
			return { parseInt<i64, vm::opargs::Immediate>(state) };
		}

#define HANDLE_LOCAL(Type)                                    \
	template<>                                                \
	auto parseArg(F8ParserState& state) -> vm::opargs::Type { \
		return { parseStr(state) };                           \
	}

		FOR_EACH(HANDLE_LOCAL, VM_OPARG_LOCAL_TYPES);

#undef HANDLE_LOCAL

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::Type {
			return { parseStr(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::FunctionName {
			return { parseStr(state) };
		}

		template<>
		auto parseArg(F8ParserState& state) -> vm::opargs::BuiltinFunctionName {
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
#include <vm/bytecode/opcode_definitions.hpp>

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
						state.log(makeBox<ExpectedSemicolonAfterError>(state.getPosition(-1)));
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
					state.log(makeBox<UnknownOpCodeError>(state.getPosition(-1), identifier1.value));
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

		/**
		    // This code will be needed in the upcoming PR
		    // https://github.com/ducktype-org/duckling/issues/699
		    state.goDown();
		    while (state.notEmpty()) {
		        tpc::Identifier field_type;
		        state.parse().one(&field_type);
		        out->parameters.emplace_back(field_type);

		        if (state.empty()) break;
		        if (state[0].is(lang_def::Special::Comma)) {
		            state.parse().one(lang_def::Special::Comma);
		        } else {
		            state.err.failAndLog(state.getPosition(), "expected comma or }");
		            state.tokens().skip();
		        }
		    }
		    state.goUpAndSkip();
		    state.parse().one(&out->result_type);

		    if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
		        state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
		        return nullptr;
		    }
		 */

		state.goDown();
		state.parse().one(&out->code);
		state.goUpAndSkip();

		return out;
	}

	namespace {
		std::vector<code::Field> parseFields(F8ParserState& state) {
			std::vector<code::Field> out;

			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
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
					state.err.failAndLog(state.getPosition(), "expected comma or }");
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
			if (!value.isNumLiteral()) {
				state.err.failAndLog(state.getPosition(), "expected number");
			} else {
				auto tp = PrimitiveType{ name, static_cast<usize>(strIDToNum(value.getValue())) };
				tp.bytecode_pos = out->position;
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
				tp.bytecode_pos = out->position;
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
					tp.bytecode_pos = out->position;
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
			tp.bytecode_pos = out->position;
			out->datatype   = std::move(tp);
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
			tp.bytecode_pos = out->position;
			out->datatype   = std::move(tp);
			break;
		}
		case lang_def::Keyword::BCOpaque: {
			lexer::Token value = state.tokens().next();
			if (!value.isNumLiteral()) {
				state.err.failAndLog(state.getPosition(), "expected number");
			} else {
				auto tp = OpaqueType{ name, static_cast<usize>(strIDToNum(value.getValue())) };
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
			base::Optional<base::StrID> extends;

			if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
				return nullptr;
			}

			state.goDown();

			while (state[0].isKeyword()) {
				auto next = state.tokens().next();
				switch (next.asKeyword()) {
				case lang_def::Keyword::BCExtends: {
					if (is_interface) {
						state.err.failAndLog(
							state.getPosition(), "interfaces cannot extend classes"
						);
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
						state.err.failAndLog(state.getPosition(), "interfaces cannot be abstract");
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
							state.err.failAndLog(state.getPosition(), "bad keyword");
							break;
						}
						state.parse().one(lang_def::Special::Semicolon);
					}
					break;
				}
				case lang_def::Keyword::BCImplements: {
					state.parse().one(lang_def::NamedOperator::Colon);
					if (!state[0].isBracketGroup(lexer::Token::BracketType::Curly)) {
						state.err.failAndLog(state.getPosition(-1), "expected `{` after here");
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
							state.err.failAndLog(state.getPosition(), "expected comma or }");
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
				case lang_def::Keyword::BCFields: {
					state.parse().one(lang_def::NamedOperator::Colon);
					// The first field is the VTable pointer
					for (auto field: parseFields(state)) fields.push_back(field);
					state.parse().one(lang_def::Special::Semicolon);
					break;
				}
				default: {
					state.err.failAndLog(state.getPosition(), "bad keyword");
					return nullptr;
				}
				}
			}

			if (state.notEmpty()) {
				state.err.failAndLog(state.getPosition(-1), "unexpected class/interface content");

				state.goUpAndSkip();
				return nullptr;
			}

			state.goUpAndSkip();

			auto tp = is_interface ? TypeOfData(InterfaceType(
										 name, std::move(implements), std::move(virtual_methods)
									 ))
			                       : TypeOfData(ClassType(
										 name,
										 fields,
										 is_abstract,
										 extends,
										 std::move(implements),
										 std::move(virtual_methods)
									 ));
			VISIT(tp, t, t.bytecode_pos = out->position);
			out->datatype = std::move(tp);
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
		VARIANT_VISIT(datatype, VISIT_CASE(auto&, data, { vm::code::serialize(data, out); }))
		out << "\n}";
	}

	void OpCode::dprint(std::ostream& out) const {
		out << "        " << opcode_name.view().stringView() << " ";
		for (auto& arg: args) {
			variant_match(arg) {
				variant_case(vm::opargs::Immediate, num_arg) { out << num_arg.value << " "; }

#define HANDLE_LOCAL(Type) \
	variant_case(vm::opargs::Type, local_type) { out << local_type.var_name.strView() << " "; }

				FOR_EACH(HANDLE_LOCAL, VM_OPARG_LOCAL_TYPES);

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

	void Func::dprint(std::ostream& out) const {
		out << "function ";
		out << name.value.strView() << "{\n";
		// Needed by: https://github.com/ducktype-org/duckling/issues/699
		// bool first = true;
		// for (const auto& param: parameters) {
		// 	if (!first) out << ", ";
		// 	out << param.value.strView();
		// 	first = false;
		// }
		// out << "} " << result_type.value.strView() << "{\n";
		code->dprint(out);
		out << "}\n";
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
