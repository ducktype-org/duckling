#pragma once

#include <iomanip>

#include "base/int_conv.hpp"
#include "instructions.hpp"
#include "elements.hpp"
#include "../../../../../../VM/src/code_data/opcode_args.hpp"
#include "preprocessor/parser/types_of_data.hpp"

namespace compiler::backend_vm {
	std::string toString(vm::opargs::ImmediateI64 arg) { return std::to_string(arg.value); }

	std::string toString(vm::opargs::StackOffset arg) { return std::to_string(arg.offset); }

	std::string toString(vm::opargs::ArgsOffset arg) { return std::to_string(arg.offset); }

	std::string toString(vm::opargs::Type arg) { return arg.type_name.str(); }

	std::string toString(vm::opargs::FunctionName arg) { return arg.function_name.str(); }

	std::string toString(vm::opargs::Label arg) { return arg.label_name.str(); }

	std::string toString0ArgOpcodeTemplate(const std::string_view opcode_name) {
		std::ostringstream oss;
		oss << std::setw(18) << std::left << opcode_name;
		oss << ";";
		return oss.str();
	}

	std::string toString1ArgOpcodeTemplate(const std::string_view opcode_name, auto arg1) {
		std::ostringstream oss;
		oss << std::setw(18) << std::left << opcode_name << " ";
		oss << std::setw(8) << toString(arg1);
		oss << ";";
		return oss.str();
	}

	std::string
		toString2ArgsOpcodeTemplate(const std::string_view opcode_name, auto arg1, auto arg2) {
		std::ostringstream oss;
		oss << std::setw(18) << std::left << opcode_name << " ";
		oss << std::setw(8) << toString(arg1) << ",";
		oss << std::setw(8) << toString(arg2);
		oss << ";";
		return oss.str();
	}

	struct InstructionSerializerVisitor {
		std::string operator()(Guardian _) { CORE_PANIC("Should not serialize Guardian"); }

#define HANDLE_OPCODE_0ARGS(opcode) \
	std::string operator()(Op_##opcode opcode) { return toString0ArgOpcodeTemplate(#opcode); }
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                   \
	std::string operator()(Op_##opcode opcode) {                 \
		return toString1ArgOpcodeTemplate(#opcode, opcode.arg0); \
	}
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                      \
	std::string operator()(Op_##opcode opcode) {                               \
		return toString2ArgsOpcodeTemplate(#opcode, opcode.arg0, opcode.arg1); \
	}

#include "../../../../../../VM/src/code_data/opcodes_list.hpp"

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
	};

	std::string toString(VmInstruction instruction) {
		return std::visit(InstructionSerializerVisitor(), instruction);
	}

	class FunctionSerializer {
		std::ostream&   out;
		const Function& function;
		i64             current_indentation = 0;

		void withIdentWriteLine(std::string_view str) {
			out << std::string(base::safeIntConv<size_t>(current_indentation), ' ') << str << "\n";
		}

		void indentUp() { current_indentation += 4; }

		void indentDown() { current_indentation -= 4; }

		void writeOptions() {
			withIdentWriteLine("stack_size: " + std::to_string(function.stack_size));
			withIdentWriteLine("arg_size: " + std::to_string(function.arg_size));
			withIdentWriteLine("next_arg_size: " + std::to_string(function.next_arg_size));
			withIdentWriteLine("ret_size: " + std::to_string(function.ret_size));
		}

		void writeCode() {
			withIdentWriteLine("code {");
			indentUp();

			for (const auto& instruction: function.body.instructions)
				withIdentWriteLine(toString(instruction));

			indentDown();
			withIdentWriteLine("}");
		}

	public:
		FunctionSerializer(std::ostream& out, const Function& function):
			  out(out),
			  function(function) {}

		void write() {
			out << "function " << function.name.strView() << " {\n";
			indentUp();

			writeOptions();
			writeCode();

			indentDown();
			out << "}\n";
		}
	};

	class TypeSerializer {
		std::ostream&         out;
		const vm::TypeOfData& type;

	private:
		struct TypeSerializerVisitor {
			std::ostream& out;

			void operator()(const vm::PrimitiveType& type) {
				out << "primitive {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    size: " << type.size << "\n";
				out << "}";
			}

			void operator()(const vm::PointerType& type) {
				out << "pointer {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    inner: " << type.inner.strView() << "\n";
				out << "}";
			}

			void operator()(const vm::StaticTableType& type) {
				out << "dynamic_table {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    inner: " << type.inner.strView() << "\n";
				out << "}";
			}

			void operator()(const vm::DynamicTableType& type) {
				out << "dynamic_table {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    inner: " << type.inner.strView() << "\n";
				out << "}";
			}

			void operator()(const vm::DataType& type) {
				out << "data {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    fields: [";
				for (auto& field: type.fields)
					out << field.name.strView() << ": " << field.type.strView() << ", ";
				out << "]\n";
				out << "}";
			}

			void operator()(const vm::VariantType& type) {
				out << "variant {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    alternatives: [";
				for (auto& alt: type.variant_alternatives) out << alt.strView() << ", ";
				out << "]\n";
				out << "}";
			}

			void operator()(const vm::FunctionType& type) {
				out << "function {\n";
				out << "    name: " << type.name.strView() << "\n";
				out << "    parameters: [";
				for (auto& param: type.parameters) out << param.strView() << ", ";
				out << "]\n";
				out << "    result: " << type.result.strView() << "\n";
				out << "}";
			}
		};

	public:
		TypeSerializer(std::ostream& out, const vm::TypeOfData& type): out(out), type(type) {}

		void write() { std::visit(TypeSerializerVisitor{ out }, type); }
	};

	void serialize(const CodeFile& file, std::ostream& out) {
		for (const auto& type: file.types) {
			TypeSerializer serializer(out, type);
			serializer.write();
		}

		for (const auto& function: file.functions) {
			FunctionSerializer serializer(out, function);
			serializer.write();
		}
	}
}
