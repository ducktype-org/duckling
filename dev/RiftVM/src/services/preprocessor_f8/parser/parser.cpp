
#include "parser.hpp"
#include <code_data/opcodes.hpp>
#include <lexer/lexer.hpp>
#include <lexer/classifications.hpp>
#include <base/optional.hpp>
#include <rift_definitions/key_spec_op.hpp>
#include <stdexcept>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/tpc.hpp>
#include <base/maps.hpp>
#include <variant>
#include <base/variant.hpp>
#include <base/str_to_int.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

namespace assemble {

	enum class OpCodeArgType { arg, local, imm };

	struct OpCodeNumArg {
		OpCodeArgType type;
		i64           value;
	};

	struct OpCodeLabelArg {
		i64         value;
		bool        type_value;
		base::StrId label_name;
	};

	using OpCodeAnyArg = std::variant<OpCodeNumArg, OpCodeLabelArg>;

	/// TODO: delete redundant using (the same as in type.hpp)
	using TypeSize = u64;

	struct PrimitiveType {
		base::StrId name;
		TypeSize    size{};

		void dprint(std::ostream& out) const {
			out << "primitive {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    size: " << size << "\n";
			out << "}";
		}
	};

	struct PointerType {
		base::StrId name;
		base::StrId inner;

		void dprint(std::ostream& out) const {
			out << "pointer {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct StaticTableType {
		base::StrId name;
		base::StrId inner;
		TypeSize    table_size;

		void dprint(std::ostream& out) const {
			out << "static_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "    table_size: " << table_size << "\n";
			out << "}";
		}
	};

	struct DynamicTableType {
		base::StrId name;
		base::StrId inner;

		void dprint(std::ostream& out) const {
			out << "dynamic_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct Field {
		base::StrId name;
		base::StrId type;
	};

	struct DataType {
		base::StrId        name;
		std::vector<Field> fields;

		void dprint(std::ostream& out) const {
			out << "data {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    fields: [";
			for (auto& field: fields)
				out << field.name.strView() << ": " << field.type.strView() << ", ";
			out << "]\n";
			out << "}";
		}
	};

	struct VariantType {
		base::StrId              name;
		std::vector<base::StrId> variant_alternatives;

		void dprint(std::ostream& out) const {
			out << "variant {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    alternatives: [";
			for (auto& alt: variant_alternatives) out << alt.strView() << ", ";
			out << "]\n";
			out << "}";
		}
	};

	struct FunctionType {
		base::StrId              name;
		std::vector<base::StrId> parameters;
		base::StrId              result;

		void dprint(std::ostream& out) const {
			out << "function {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    parameters: [";
			for (auto& param: parameters) out << param.strView() << ", ";
			out << "]\n";
			out << "    result: " << result.strView() << "\n";
			out << "}";
		}
	};

	using TypeData = std::variant<
		PrimitiveType,
		PointerType,
		StaticTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType>;

	struct AsmElement: tpc::Element {};

	struct Type: AsmElement {
		TypeData                    datatype;
		static tpc::ParserRef<Type> parse(tpc::ParserState& state);

		void dprint(std::ostream& out) const override {
			out << "type: ";
			VARIANT_VISIT(datatype, VISIT_CASE(auto&, data, { data.dprint(out); }))
			out << "\n}";
		}
	};

	struct OpCode: AsmElement {
		base::StrId               opcode_name;
		std::vector<OpCodeAnyArg> args;

		static tpc::ParserRef<OpCode> parse(tpc::ParserState& state) {
			auto out = tpc::makeRef<OpCode>();

			tpc::Identifier identifier1;
			tpc::parseOne(state, &identifier1);
			out->opcode_name = identifier1.value;

			if (state.ctokens().peek().is(rift_def::Special::Semicolon)) {
				tpc::parseOne(state, rift_def::Special::Semicolon);
				return out;
			}


			while (state.notEmpty()) {
				switch (state.ctokens().peek().getType()) {
				case lexer::Token::Type::Identifier: {
					tpc::Identifier identifier2;
					tpc::parseOne(state, &identifier2);

					out->args.emplace_back(OpCodeLabelArg{ 0, false, identifier2.value });
					break;
				}
				case lexer::Token::Type::NumLiteral:
					try {
						out->args.emplace_back(OpCodeNumArg{
							OpCodeArgType::imm, base::strIdToNum(state.tokens().next().getValue()) }
						);
					} catch (std::logic_error& e) {
						state.err.setFail();
						state.err.logError(
							state.ctokens().peek().getPosition(),
							base::strConcat("not a number: ", state.ctokens().peek().getValue())
						);
						state.tokens().skip();
					}
					break;
				default:
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(),
						base::strConcat("incorrect token1: ", state.ctokens().peek().getValue())
					);
					state.tokens().skip();
				}

				if (state.ctokens().peek().is(rift_def::Special::Semicolon)) {
					tpc::parseOne(state, rift_def::Special::Semicolon);
					break;
				}

				if (state.ctokens().peek().is(rift_def::Operator::Comma)) {
					tpc::parseOne(state, rift_def::Operator::Comma);
				} else {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(),
						base::strConcat("incorrect token2: ", state.ctokens().peek().getValue())
					);
					state.tokens().skip();
				}
			}

			return out;
		}

		void dprint(std::ostream& out) const override {
			out << "        " << opcode_name.view().stringView() << " ";
			for (auto& arg: args) {
				variant_match(arg) {
					variant_case(OpCodeNumArg, num_arg) { out << num_arg.value << " "; }
					variant_case(OpCodeLabelArg, label_arg) {
						out << label_arg.label_name.strView() << " (" << label_arg.value << ")"
							<< " ";
					}
				}
			}
			out << '\n';
		}

		~OpCode() override = default;
	};

	struct ByteCode: AsmElement {
		std::vector<tpc::ParserRef<OpCode>> opcodes;
		base::Map<base::StrId, usize>       label_position;

		static tpc::ParserRef<ByteCode> parse(tpc::ParserState& state) {
			auto out = tpc::makeRef<ByteCode>();

			while (state.notEmpty()) {
				if (state.tryEat(rift_def::Keyword::BCLabel)) {
					tpc::Identifier label_name;
					tpc::parseOne(state, &label_name);

					if (!state.tryEat(rift_def::Special::Semicolon)) {
						state.err.logError(
							state.ctokens().peek(-1).getPosition(), "semicolon expected"
						);
					}

					if (out->label_position.contains(label_name.value)) {
						state.err.setFail();
						state.err.logError(
							state.ctokens().peek(-1).getPosition(),
							base::strConcat("repeated label: ", label_name.value)
						);
					} else {
						out->label_position.put(label_name.value, out->opcodes.size());
					}
				} else {
					out->opcodes.emplace_back(OpCode::parse(state));
				}
			}

			for (i32 i = 0; i < out->opcodes.size(); i++) {
				for (auto& opcode: out->opcodes[i]->args) {
					variant_match(opcode) {
						variant_case(OpCodeLabelArg, label) {
							auto it = out->label_position.find(label.label_name);
							if (it != out->label_position.end()) {
								label.value      = static_cast<i64>(it->second) - i - 1;
								label.type_value = false;
							} else {
								// @TODO: not failing here allow for "type arguments"
								// This setup should be changed in the future.
								label.type_value = true;

								// state.err.setFail();
								// state.err.logError(
								// 	state.ctokens().peek().getPosition(),
								// 	base::strConcat("Nonexistent label: ",
								//                      std::get<OpCodeLabelArg>(opcode).value));
							}
						}
					}
				}
			}

			return out;
		}

		void dprint(std::ostream& out) const override {
			out << "    code {\n";
			for (auto& opcode: opcodes) opcode->dprint(out);
			out << "    }\n";
		}

		~ByteCode() override = default;
	};

	constexpr usize size_t_max = (usize) (-1);

	struct Func: AsmElement {
		tpc::Identifier          name;
		usize                    arg_size   = size_t_max;
		usize                    local_size = size_t_max;
		usize                    ret_size   = size_t_max;
		tpc::ParserRef<ByteCode> code;

		static tpc::ParserRef<Func> parse(tpc::ParserState& state);

		void dprint(std::ostream& out) const override {
			out << "function {\n";
			out << "    name: " << name.value.strView() << "\n";
			out << "    arg_size: " << arg_size << "\n";
			out << "    local_size: " << local_size << "\n";
			out << "    ret_size: " << ret_size << "\n";
			code->dprint(out);
			out << "\n}";
		}

		~Func() override = default;
	};

	struct ParsedCode: AsmElement {
		std::vector<tpc::ParserRef<Func>> functions;
		std::vector<tpc::ParserRef<Type>> types;

		static tpc::ParserRef<ParsedCode> parse(tpc::ParserState& state);

		void dprint(std::ostream& out) const override {
			for (auto& type: types) {
				type->dprint(out);
				out << "\n";
			}

			for (auto& func: functions) {
				func->dprint(out);
				out << "\n";
			}
		}

		~ParsedCode() override = default;
	};

	tpc::ParserRef<Func> Func::parse(tpc::ParserState& state) {
		auto out = tpc::makeRef<Func>();

		tpc::parseAll(state, rift_def::Keyword::BCFunction, &out->name);

		if (!state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly)) {
			state.err.setFail();
			state.err.logError(state.ctokens().peek(-1).getPosition(), "expected `{` after here");
			return nullptr;
		}

		state.goDown();

		while (state.ctokens().peek().isKeyword()) {
			auto next = state.tokens().next();

			switch (next.asKeyword()) {
			case rift_def::Keyword::BCArgSize: {
				tpc::parseOne(state, rift_def::Operator::Colon);
				if (out->arg_size != size_t_max) {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "arg_size duplicate");
				}
				auto value = state.tokens().next();

				if (!value.isNumLiteral()) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(), "arg_size argument is not num-literal"
					);
				}
				try {
					out->arg_size = strIdToNum(value.getValue());
				} catch (std::logic_error& e) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(), "arg_size argument is not num-literal"
					);
				}
				tpc::parseOne(state, rift_def::Special::Semicolon);
				break;
			}

			case rift_def::Keyword::BCLocalSize: {
				tpc::parseOne(state, rift_def::Operator::Colon);
				if (out->local_size != size_t_max) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(), "local_size duplicate"
					);
				}
				auto value = state.tokens().next();
				if (!value.isNumLiteral()) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(),
						"local_size argument is not num-literal"
					);
				}
				try {
					out->local_size = strIdToNum(value.getValue());
				} catch (std::logic_error& e) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(),
						"local_size argument is not num-literal"
					);
				}

				tpc::parseOne(state, rift_def::Special::Semicolon);
				break;
			}

			case rift_def::Keyword::BCRetSize: {
				tpc::parseOne(state, rift_def::Operator::Colon);
				if (out->ret_size != size_t_max) {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "ret_size duplicate");
				}
				auto value = state.tokens().next();
				if (!value.isNumLiteral()) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(), "ret_size argument is not num-literal"
					);
				}
				try {
					out->ret_size = base::strIdToNum(value.getValue());
				} catch (std::logic_error& e) {
					state.err.setFail();
					state.err.logError(
						state.ctokens().peek().getPosition(), "ret_size argument is not num-literal"
					);
				}

				tpc::parseOne(state, rift_def::Special::Semicolon);
				break;
			}

			case rift_def::Keyword::BCDefine: {
				throw base::NotYetImplemented("BCDefine");
			}

			case rift_def::Keyword::BCCode: {
				tpc::parseOne(state, rift_def::Operator::Colon);
				if (!state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly)) {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "no {} on code:");
				}

				state.goDown();
				tpc::parseOne(state, &out->code);
				state.goUpAndSkip();
				break;
			}

			default:
				state.err.setFail();
				state.err.logError(state.ctokens().peek().getPosition(), "bad keyword in function");
			}
		}

		if (state.notEmpty()) {
			state.err.setFail();
			state.err.logError(
				state.ctokens().peek(-1).getPosition(), "Unexpected function content!"
			);

			state.goUpAndSkip();
			return nullptr;
		}

		state.goUpAndSkip();

		if (out->local_size == size_t_max) state.fail(0, "Local size not set");
		if (out->arg_size == size_t_max) state.fail(0, "Arg size not set");
		if (out->ret_size == size_t_max) state.fail(0, "Ret size not set");

		return out;
	}

	tpc::ParserRef<Type> Type::parse(tpc::ParserState& state) {
		if (!state.tryEat(rift_def::Keyword::BCType)) {
			state.err.setFail();
			state.err.logError(state.ctokens().peek().getPosition(), "expected keyword 'type'");
			return nullptr;
		};
		auto out = tpc::makeRef<Type>();

		/// @TODO: implement keywordToNumLiteral
		auto type = state.tokens().next().asKeyword();
		tpc::parseOne(state, rift_def::Operator::Colon);
		base::StrId name = state.tokens().next().getValue();

		switch (type) {
		case rift_def::Keyword::BCPrimitive: {
			auto value = state.tokens().next();
			if (!value.isNumLiteral()) {
				state.err.setFail();
				state.err.logError(state.ctokens().peek().getPosition(), "expected number");
			} else {
				out->datatype
					= PrimitiveType{ name, static_cast<TypeSize>(strIdToNum(value.getValue())) };
			}
			break;
		}
		case rift_def::Keyword::BCPointer: {
			auto pointered_type = state.tokens().next();
			if (!pointered_type.isIdentifier()) {
				state.err.setFail();
				state.err.logError(state.ctokens().peek().getPosition(), "expected identifier");
			} else {
				out->datatype = PointerType{ name, pointered_type.getValue() };
			}
			break;
		}
		case rift_def::Keyword::BCStaticTable: {
			auto type_name = state.tokens().next();
			if (!type_name.isIdentifier()) {
				state.err.setFail();
				state.err.logError(state.ctokens().peek().getPosition(), "expected identifier");
			} else {
				auto size = state.tokens().next();
				if (!size.isNumLiteral()) {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "expected number");
				} else {
					out->datatype
						= StaticTableType{ name,
						                   type_name.getValue(),
						                   static_cast<TypeSize>(strIdToNum(size.getValue())) };
				}
			}
			break;
		}
		case rift_def::Keyword::BCDynamicTable: {
			const lexer::Token& type_name = state.tokens().next();
			if (!type_name.isIdentifier()) {
				state.err.setFail();
				state.err.logError(state.ctokens().peek().getPosition(), "expected identifier");
			} else {
				out->datatype = DynamicTableType{ name, type_name.getValue() };
			}
			break;
		}
		case rift_def::Keyword::BCData: {
			if (!state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.setFail();
				state.err.logError(
					state.ctokens().peek(-1).getPosition(), "expected `{` after here"
				);
				return nullptr;
			}

			state.goDown();
			std::vector<Field> fields;
			while (state.notEmpty()) {
				tpc::Identifier field_name;
				tpc::Identifier field_type;
				tpc::parseAll(state, &field_name, rift_def::Operator::Colon, &field_type);
				fields.emplace_back(Field{ field_name.value, field_type.value });

				if (state.empty()) break;

				if (state.ctokens().peek().is(rift_def::Operator::Comma)) {
					tpc::parseOne(state, rift_def::Operator::Comma);
				} else {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			out->datatype = DataType{ name, fields };
			break;
		}
		case rift_def::Keyword::BCVariant: {
			if (!state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.setFail();
				state.err.logError(
					state.ctokens().peek(-1).getPosition(), "expected `{` after here"
				);
				return nullptr;
			}

			state.goDown();
			std::vector<base::StrId> alternatives;
			while (state.notEmpty()) {
				tpc::Identifier field_type;
				tpc::parseOne(state, &field_type);
				alternatives.emplace_back(field_type.value);

				if (state.empty()) break;
				if (state.ctokens().peek().is(rift_def::Operator::Comma)) {
					tpc::parseOne(state, rift_def::Operator::Comma);
				} else {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			out->datatype = VariantType{ name, alternatives };
			break;
		}
		case rift_def::Keyword::BCFunType: {
			if (!state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly)) {
				state.err.setFail();
				state.err.logError(
					state.ctokens().peek(-1).getPosition(), "expected `{` after here"
				);
				return nullptr;
			}

			state.goDown();
			std::vector<base::StrId> arguments;
			while (state.notEmpty()) {
				tpc::Identifier field_type;
				tpc::parseOne(state, &field_type);
				arguments.emplace_back(field_type.value);

				if (state.empty()) break;
				if (state.ctokens().peek().is(rift_def::Operator::Comma)) {
					tpc::parseOne(state, rift_def::Operator::Comma);
				} else {
					state.err.setFail();
					state.err.logError(state.ctokens().peek().getPosition(), "expected comma or }");
					state.tokens().skip();
				}
			}
			state.goUpAndSkip();
			tpc::Identifier result;
			tpc::parseOne(state, &result);
			out->datatype = FunctionType{ name, arguments, result };
			break;
		}
		default: {
			state.err.setFail();
			state.err.logError(state.ctokens().peek().getPosition(), "expected variant of type");
			break;
		}
		}

		return out;
	}

	tpc::ParserRef<ParsedCode> ParsedCode::parse(tpc::ParserState& state) {
		auto out = tpc::makeRef<ParsedCode>();

		while (state.notEmpty()) {
			if (state.ctokens().is(rift_def::Keyword::BCType)) {
				auto type = Type::parse(state);
				if (type != nullptr) out->types.emplace_back(std::move(type));
			} else if (state.ctokens().is(rift_def::Keyword::BCFunction)) {
				auto func = Func::parse(state);
				if (func != nullptr) out->functions.emplace_back(std::move(func));
			} else {
				state.fail(0, "Unexpected keyword");
				return out;
			}
		}

		return out;
	}

	struct CodeContainer {
		// @TODO: https://github.com/rift-lang/rift-poc-zpp1/issues/70
		bool                       ok    = true;
		std::string                error = "";
		tpc::ParserRef<ParsedCode> code;
		void                       print(std::ostream&);
	};

	CodeContainer parseFile(const fs::FilePath& path) {
		lexer::init();
		tpc::init();
		rift_def::setKeywordMode(rift_def::KeywordMode::RiftBC);

		auto maybeContent = path.getContentSafe();
		if (maybeContent.has_error()) return CodeContainer{ false, maybeContent.error(), nullptr };

		lexer::TokenData td = lexer::tokenizeFile(path);

		tpc::ParserState state(
			tpc::TokenStream(td.tokens, tpc::Token(td.eof_sentinel), 0, td.tokens.size()),
			dia::ErrorState()
		);

		tpc::ParserRef<assemble::ParsedCode> out = ParsedCode::parse(state);

		std::stringstream err_stream;
		state.err.dumpLog(err_stream);

		return { state.err.good(), err_stream.str(), std::move(out) };
	}

	// returns true if was successfully
	bool defineTypes(CodeContainer& code, vm::TypeMetadata& type_metadata) {
		base::Map<base::StrId, vm::TypeRef> type_map;

		for (auto& type: code.code->types) {
			base::StrId name = VISIT(type->datatype, value, return value.name);

			if (type_map.contains(name)) {
				code.ok = false;
				code.error += base::strConcat("Error: repeated type: ", name, "\n");

				/// Possible we can allow to continue
				return false;
			}

			vm::Type typ      = vm::Type::declareType(name);
			auto     type_ref = type_metadata.addType(std::move(typ));
			type_map.put(name, type_ref);
		}

		for (auto& type: code.code->types) {
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
					std::vector<std::pair<base::StrId COMMA vm::TypeRef>> fields;
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
				variant_default { RIFT_PANIC("bad type"); }
			}
		}

		type_metadata.finalize();

		return true;
	}

	void CodeContainer::print(std::ostream& out) {
		code->dprint(out);
		if (!ok) out << error << '\n';
	}

	u16 nameToOpcodeValue(base::StrId str) {
		try {
			return static_cast<u16>(base::strToEnum<vm::OpcodeFix8>(str));
		} catch (std::out_of_range& err) {
			// @TODO: better errors
			RIFT_PANIC(base::strConcat("Incorrect opcode: ", str));
			return 0;
		}
	}

	vm::FuncData
		changeFuncToFuncData(const tpc::ParserCBorrowRef<Func>& func, vm::TypeMetadata& types) {
		vm::FuncData funcData;
		funcData.ret_size   = 0;
		funcData.arg_size   = func->arg_size;
		funcData.stack_size = func->local_size;
		funcData.ret_size   = func->ret_size;

		for (auto& op: func->code->opcodes) {
			// calculate type arguments:
			for (auto& arg: op->args) {
				variant_match(arg) {
					variant_case(OpCodeLabelArg, label) {
						if (label.type_value) {
							auto type = types.getTypeByName(label.label_name);
							if (!type.has_value()) {
								std::cerr << "Wrong type name! " << label.label_name.strView()
										  << "\n";
								label.value = 0;
							} else {
								label.value = static_cast<i64>(u64(type.value()->getId()));
							}
						}
					}
				}
			}

			i64 arg_0 = 0;
			i64 arg_1 = 0;
			switch (op->args.size()) {
			case 0: {
				break;
			}
			case 1: {
				std::visit([&arg_0](auto& arg) { arg_0 = arg.value; }, op->args[0]);
				break;
			}
			case 2: {
				std::visit([&arg_0](auto& arg) { arg_0 = arg.value; }, op->args[0]);
				std::visit([&arg_1](auto& arg) { arg_1 = arg.value; }, op->args[1]);
				break;
			}
			}


#ifdef USE_TAIL_CALLS
			funcData.bc.emplace_back(vm::Fix8Instruction{
				.opfun = vm::OpFuns::opfuns[nameToOpcodeValue(op->opcode_name)],
				.arg0  = static_cast<i32>(arg_0),
				.arg1  = static_cast<i32>(arg_1) });

#else
			funcData.bc.emplace_back(vm::Fix8Instruction{
				.opcode = static_cast<u16>(nameToOpcodeValue(op->opcode_name)),
				.arg0   = static_cast<i32>(arg_0),
				.arg1   = static_cast<i32>(arg_1) });
#endif
		}
		return funcData;
	}

	// @TODO: this function returns errors as string, in the future `Console` like object should be
	// returned, that can produce both human readable and json error output
	cpp::result<vm::Code, std::string>
		getCode(CodeContainer& code, vm::TypeMetadata& type_metadata) {
		if (!code.ok) return cpp::failure(code.error);

		vm::Code instructions_code;

		usize main_id = SIZE_MAX;

		for (i32 idx = 0; idx < code.code->functions.size(); idx++) {
			if (code.code->functions[idx]->name.value.strView() == "main") {
				main_id = idx;
				break;
			}
		}

		if (main_id == SIZE_MAX) return cpp::failure("error: No main.");
		instructions_code.main_id = main_id;

		for (auto& func: code.code->functions) {
			instructions_code.functions.emplace_back(
				changeFuncToFuncData(func.borrow(), type_metadata)
			);
		}

		return instructions_code;
	}

	cpp::result<vm::Code, std::string>
		assemble(const fs::FilePath& file, vm::TypeMetadata& type_metadata) {
		auto parsed_code = parseFile(file);

		// @TODO:
		// it is left, because JSON is broken
		std::cerr << parsed_code.error;

		if (!parsed_code.ok) return cpp::fail(parsed_code.error);

		bool status = defineTypes(parsed_code, type_metadata);
		if (!status) return cpp::fail(parsed_code.error);

		auto result = getCode(parsed_code, type_metadata);

		// parsed_code.print(std::cerr);

		return result;
	}

}
