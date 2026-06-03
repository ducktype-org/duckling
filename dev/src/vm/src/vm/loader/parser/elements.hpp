#pragma once

#include <diagnostic_interactive/logger.hpp>

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/parser_state.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <vm/bytecode/const_pool.hpp>

namespace vm::loader::parser {

	class F8ParserState final: public tpc::ParserState {
	public:
		F8ParserState(tpc::TokenStream&& stream, Ref<dia_int::Logger> err_int):
			  tpc::ParserState(std::move(stream), err_int) {}

		tpc::GenericAutomatic<F8ParserState> parse();
	};

	struct AsmElement: tpc::Element {
		dia::SourcePosition position;

		AsmElement(const dia::SourcePosition& position): position(position) {}

		void debugPrint(std::ostream& out) const override;
	};

	struct Type final: AsmElement {
		using AsmElement::AsmElement;

		code::TypeOfData  datatype;
		static MBox<Type> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;
	};

	struct GlobalData final: AsmElement {
		using AsmElement::AsmElement;

		tpc::Identifier                 name;
		tpc::Identifier                 type;
		base::Optional<tpc::Identifier> ctor_name;
		base::Optional<tpc::Identifier> dtor_name;

		static Box<GlobalData> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;
	};

	struct ConstantData final: AsmElement {
		using AsmElement::AsmElement;

		tpc::Identifier    name;
		tpc::Identifier   type;
		code::ConstantValue value;

		static MBox<ConstantData> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;
	};

	// @TODO: #1705 Figure out a better name
	struct OpCode final: AsmElement {
		using AsmElement::AsmElement;

		base::StrID opcode_name;

		std::vector<opargs::OpCodeArg> args;

		static MBox<OpCode> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;

		~OpCode() override = default;
	};

	struct ByteCode final: AsmElement {
		using AsmElement::AsmElement;

		std::vector<Box<OpCode>> opcodes;

		static Box<ByteCode> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;

		~ByteCode() override = default;
	};

	struct Func final: AsmElement {
		using AsmElement::AsmElement;

		tpc::Identifier              name;
		std::vector<tpc::Identifier> parameters;
		std::vector<tpc::Identifier> result_types;
		MBox<ByteCode>               code;

		static MBox<Func> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;

		~Func() override = default;
	};

	struct ParsedFile final {
		std::vector<Box<Func>>        functions;
		std::vector<Box<Type>>        types;
		std::vector<Box<GlobalData>>  global_data;
		std::vector<Box<ConstantData>> constants;
		fs::File                      source_file;

		ParsedFile(fs::File source_file);

		static MBox<ParsedFile> parse(F8ParserState& state);
		void                    dprint(std::ostream& out) const;
	};
}
