#pragma once

#include <diagnostic/source_position.hpp>
#include <base/box.hpp>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <code_data/opcode_args.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
#include <token_file/file.hpp>
#include <token_parser_core/common_elements.hpp>
#include "type_data.hpp"
#include <token_parser_core/tpc.hpp>
#include <token_parser_core/base_element.hpp>
#include <base/variant.hpp>
#include <vector>

namespace vm::parser {

	class F8ParserState;

	struct OpCodeArgAndPosition {
		vm::opargs::OpCodeArg arg;
		dia::SourcePosition   position;
	};

	struct AsmElement: tpc::Element {
		base::Box<dia::SourcePosition> position;

		AsmElement(const dia::SourcePosition& position):
			  position(makeBox<dia::SourcePosition>(position)) {}

		void debugPrint(std::ostream& out) const override { dprint(out); }
	};

	struct Type: AsmElement {
		using AsmElement::AsmElement;

		TypeData          datatype;
		static MBox<Type> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override {
			out << "type: ";
			VARIANT_VISIT(datatype, VISIT_CASE(auto&, data, { data.dprint(out); }))
			out << "\n}";
		}
	};

	struct OpCode: AsmElement {
		using AsmElement::AsmElement;

		base::StrID opcode_name;

		std::vector<OpCodeArgAndPosition> args;

		static MBox<OpCode> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override {
			out << "        " << opcode_name.view().stringView() << " ";
			for (auto& arg: args) {
				variant_match(arg.arg) {
					variant_case(vm::opargs::ImmediateI64, num_arg) { out << num_arg.value << " "; }
					variant_case(vm::opargs::StackOffset, stack_offset_arg) {
						out << stack_offset_arg.offset << " ";
					}
					variant_case(vm::opargs::ArgsOffset, args_offset_arg) {
						out << args_offset_arg.offset << " ";
					}
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

		~OpCode() override = default;
	};

	struct ByteCode: AsmElement {
		using AsmElement::AsmElement;

		std::vector<Box<OpCode>>      opcodes;
		base::Map<base::StrID, usize> label_position;

		static Box<ByteCode> parse(F8ParserState& state);

		void dprint(std::ostream& out) const override {
			out << "    code {\n";
			for (auto& opcode: opcodes) opcode->dprint(out);
			out << "    }\n";
		}

		~ByteCode() override = default;
	};

	constexpr usize SIZE_T_MAX = std::numeric_limits<usize>::max();

	struct Func: AsmElement {
		using AsmElement::AsmElement;

		tpc::Identifier name;
		usize           arg_size      = SIZE_T_MAX;
		usize           next_arg_size = SIZE_T_MAX;
		usize           local_size    = SIZE_T_MAX;
		usize           ret_size      = SIZE_T_MAX;
		MBox<ByteCode>  code;

		static MBox<Func> parse(F8ParserState& state);

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

	struct ParsedFile: AsmElement {
		using AsmElement::AsmElement;

		std::vector<Box<Func>> functions;
		std::vector<Box<Type>> types;

		static MBox<ParsedFile> parse(F8ParserState& state);

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

		~ParsedFile() override = default;
	};

	/**
	 * @brief Structure containing a parsed program combined
	 * from all files with type metadata.
	 */
	struct ParsedProgram {
		std::vector<base::Box<dia::SourcePosition>> files_src_pos;
		base::HashMap<base::StrID, Ref<Func>>       name_to_func;
		std::vector<Box<Func>>                      functions;
		std::vector<Box<Type>>                      types;
		std::vector<Box<tokenizer::TokenFile>>      token_files;
		Box<vm::TypeMetadata>                       type_metadata = makeBox<vm::TypeMetadata>();

		void dprint(std::ostream& out) const {
			for (auto& type: types) {
				type->dprint(out);
				out << "\n";
			}

			for (auto& func: functions) {
				func->dprint(out);
				out << "\n";
			}
		}
	};
}
