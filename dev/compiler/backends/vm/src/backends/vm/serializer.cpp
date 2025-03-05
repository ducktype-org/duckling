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

	void writeComment(const std::string_view comment_content, std::ostream& out) {
		out << '#' << ' ' << comment_content;
	}

	void write0ArgOpcodeTemplate(const std::string_view opcode_name, std::ostream& out) {
		out << opcode_name;
		out << ";";
	}

	void write1ArgOpcodeTemplate(const std::string_view opcode_name, auto arg1, std::ostream& out) {
		out << std::setw(17) << std::left << opcode_name << " ";
		out << std::setw(8) << std::right << toString(arg1);
		out << ";";
	}

	void write2ArgsOpcodeTemplate(
		const std::string_view opcode_name, auto arg1, auto arg2, std::ostream& out
	) {
		out << std::setw(17) << std::left << opcode_name << " ";
		out << std::setw(8) << std::right << toString(arg1) << ",";
		out << std::setw(8) << std::right << toString(arg2);
		out << ";";
	}

	struct InstructionSerializerVisitor {
		std::ostream& out;

		void operator()(Guardian) { CORE_PANIC("Should not serialize Guardian"); }

		void operator()(const Comment& comment) { writeComment(comment.comment.strView(), out); }

#define HANDLE_OPCODE_0ARGS(opcode) \
	void operator()(Op_##opcode) { write0ArgOpcodeTemplate(#opcode, out); }
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) \
	void operator()(Op_##opcode opcode) { write1ArgOpcodeTemplate(#opcode, opcode.arg0, out); }
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                 \
	void operator()(Op_##opcode opcode) {                                 \
		write2ArgsOpcodeTemplate(#opcode, opcode.arg0, opcode.arg1, out); \
	}

#include "../../../../../../VM/src/code_data/opcodes_list.hpp"

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
	};

	void writeInstruction(VmInstruction instruction, std::ostream& out) {
		std::visit(InstructionSerializerVisitor{ out }, instruction);
	}

	class FunctionSerializer {
		std::ostream&   out;
		const Function& function;
		i64             current_indentation = 0;

		void withIdentWriteLine(const std::function<void(std::ostream&)>& write) {
			out << std::string(base::safeIntConv<size_t>(current_indentation), ' ');
			write(out);
			out << "\n";
		}

		void withIdentWriteLine(std::string_view str) {
			withIdentWriteLine([&](std::ostream& out) { out << str; });
		}

		void indentUp() { current_indentation += 4; }

		void indentDown() { current_indentation -= 4; }

		void writeOption(const std::string_view name, usize value) {
			withIdentWriteLine([&](std::ostream& out) { out << name << ": " << value << ";"; });
		}

		void writeOptions() {
			writeOption("local_size", function.stack_size);
			writeOption("arg_size", function.arg_size);
			writeOption("next_arg_size", function.next_arg_size);
			writeOption("ret_size", function.ret_size);
		}

		void writeCode() {
			withIdentWriteLine("code: {");
			indentUp();

			for (const auto& instruction: function.body)
				withIdentWriteLine([&](std::ostream& out) { writeInstruction(instruction, out); });

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
			out << '\n';
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
				out << "type primitive: ";
				out << type.name.strView() << " ";
				out << type.size;
			}

			void operator()(const vm::PointerType&) {
				throw base::NotYetImplemented("PointerType serialization");
			}

			void operator()(const vm::StaticTableType& type) {
				out << "type static_table: ";
				out << type.name.strView() << " ";
				out << type.inner.strView() << " ";
				out << type.table_size;
			}

			void operator()(const vm::DynamicTableType&) {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const vm::DataType&) {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const vm::VariantType&) {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const vm::FunctionType&) {
				throw base::NotYetImplemented("DynamicTableType serialization");
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
			out << '\n';
		}

		out << '\n';

		for (const auto& function: file.functions) {
			FunctionSerializer serializer(out, function);
			serializer.write();
			out << '\n';
		}
	}
}
