#pragma once

#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>
#include <base/variant.hpp>
#include <code_data/opcode_args.hpp>
#include <code_data/opcodes.hpp>
#include <core/process/type_metadata/type_metadata.hpp>

namespace assemble {

	/*
	 * All parser types are declared here so we can access them in other modules.
	 * For example: validator.hpp
	 */
	constexpr usize SIZE_T_MAX = std::numeric_limits<usize>::max();

	class F8ParserState final: public tpc::ParserState {
	public:
		F8ParserState(tpc::TokenStream&& stream, dia::Logger& err):
			  tpc::ParserState(std::move(stream), err) {}

		tpc::GenericAutomatic<F8ParserState> parse() { return { *this }; }
	};

	struct PrimitiveType {
		base::StrID name;
		usize       size{};

		void dprint(std::ostream& out) const {
			out << "primitive {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    size: " << size << "\n";
			out << "}";
		}
	};

	struct PointerType {
		base::StrID name;
		base::StrID inner;

		void dprint(std::ostream& out) const {
			out << "pointer {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct StaticTableType {
		base::StrID name;
		base::StrID inner;
		usize       table_size;

		void dprint(std::ostream& out) const {
			out << "static_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "    table_size: " << table_size << "\n";
			out << "}";
		}
	};

	struct DynamicTableType {
		base::StrID name;
		base::StrID inner;

		void dprint(std::ostream& out) const {
			out << "dynamic_table {\n";
			out << "    name: " << name.strView() << "\n";
			out << "    inner: " << inner.strView() << "\n";
			out << "}";
		}
	};

	struct Field {
		base::StrID name;
		base::StrID type;
	};

	struct DataType {
		base::StrID        name;
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
		base::StrID              name;
		std::vector<base::StrID> variant_alternatives;

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
		base::StrID              name;
		std::vector<base::StrID> parameters;
		base::StrID              result;

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

	struct OpCodeArgAndPosition {
		vm::opargs::OpCodeArg arg;
		dia::SourcePosition   position;
	};

	using TypeData = std::variant<
		PrimitiveType,
		PointerType,
		StaticTableType,
		DynamicTableType,
		DataType,
		VariantType,
		FunctionType>;

	struct AsmElement: tpc::Element {
		dia::SourcePosition position;

		AsmElement(const dia::SourcePosition& position): position(position) {}

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

		dia::SourcePosition position = dia::SourcePosition::fakePosition(); // @TODO: How should that be interpreted in case of
		// multiple files?
		// base::HashMap<Ref<Func>, base::FilePath> map;
		std::vector<Box<Func>> functions;
		std::vector<Box<Type>> types;
		vm::TypeMetadata       type_metadata;

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

	MBox<ParsedProgram>
		assemble(const std::vector<fs::FilePath>& files, dia::Logger& log);

	std::expected<Box<vm::VMProgram>, std::string>
		changeParsedProgramToVMProgram(Ref<ParsedProgram> parsed_program, dia::Logger& log);
}

using assemble::ByteCode;
using assemble::Func;
using assemble::OpCode;
using assemble::ParsedProgram;
using assemble::Type;
