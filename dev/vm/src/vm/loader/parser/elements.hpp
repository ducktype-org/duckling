#pragma once

#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>
#include <token_file/file.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/token_stream.hpp>

#include <base/box.hpp>
#include <base/macros/for_each.hpp>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::loader::parser {

	class F8ParserState final: public tpc::ParserState {
	public:
		F8ParserState(tpc::TokenStream&& stream, dia::Logger& err):
			  tpc::ParserState(std::move(stream), err) {}

		tpc::GenericAutomatic<F8ParserState> parse();
	};

	struct AsmElement: tpc::Element {
		Box<dia::SourcePosition> position;

		AsmElement(const dia::SourcePosition& position):
			  position(makeBox<dia::SourcePosition>(position)) {}

		void debugPrint(std::ostream& out) const override;
	};

	struct Type final: AsmElement {
		using AsmElement::AsmElement;

		code::TypeOfData  datatype;
		static MBox<Type> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;
	};

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

	constexpr usize SIZE_T_MAX = std::numeric_limits<usize>::max();

	struct Func final: AsmElement {
		using AsmElement::AsmElement;

		tpc::Identifier name;
		usize           arg_size   = SIZE_T_MAX;
		usize           local_size = SIZE_T_MAX;
		usize           ret_size   = SIZE_T_MAX;
		MBox<ByteCode>  code;

		static MBox<Func> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override;

		~Func() override = default;
	};

	struct ParsedFile final {
		std::vector<Box<Func>> functions;
		std::vector<Box<Type>> types;
		fs::FilePath           source_file;

		ParsedFile(fs::FilePath source_file);

		static MBox<ParsedFile> parse(F8ParserState& state);
		void                    dprint(std::ostream& out) const;
	};
}
