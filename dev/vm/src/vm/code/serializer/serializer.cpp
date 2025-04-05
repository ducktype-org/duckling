#include "serializer.hpp"

#include <base/for_each.hpp>
#include <base/int_conv.hpp>

#include <vm/code/code.hpp>
#include <vm/code/type_of_data.hpp>

#include <iomanip>

namespace vm::code {
	std::string toString(opargs::Immediate arg) { return std::to_string(arg.value); }

#define OFFSET_TO_STRING(Tp) \
	std::string toString(vm::opargs::Tp arg) { return std::to_string(arg.offset); }

	FOR_EACH(OFFSET_TO_STRING, VM_OPARG_OFFSET_TYPES);

	std::string toString(opargs::Type arg) { return arg.type_name.str(); }

	std::string toString(opargs::FunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::Label arg) { return arg.label_name.str(); }

	void writeComment(const std::string_view comment_content, std::ostream& out) {
		out << '#' << ' ' << comment_content;
	}

	void write0ArgOpcodeTemplate(const std::string_view opcode_name, std::ostream& out) {
		out << opcode_name;
		out << ";";
	}

	void write1ArgOpcodeTemplate(const std::string_view opcode_name, auto arg1, std::ostream& out) {
		out << std::setw(22) << std::left << opcode_name << " ";
		out << std::setw(8) << std::right << toString(arg1);
		out << ";";
	}

	void write2ArgsOpcodeTemplate(
		const std::string_view opcode_name, auto arg1, auto arg2, std::ostream& out
	) {
		out << std::setw(22) << std::left << opcode_name << " ";
		out << std::setw(8) << std::right << toString(arg1) << ",";
		out << std::setw(8) << std::right << toString(arg2);
		out << ";";
	}

	struct InstructionSerializerVisitor {
		std::ostream& out;

		void operator()(const instructions::Comment& comment) const {
			writeComment(comment.comment.strView(), out);
		}

#define HANDLE_OPCODE_0ARGS(opcode) \
	void operator()(VM_INSTR_FROM_NAME(opcode)) const { write0ArgOpcodeTemplate(#opcode, out); }
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                 \
	void operator()(VM_INSTR_FROM_NAME(opcode) opcode) const { \
		write1ArgOpcodeTemplate(#opcode, opcode.arg0, out);    \
	}
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                 \
	void operator()(VM_INSTR_FROM_NAME(opcode) opcode) const {            \
		write2ArgsOpcodeTemplate(#opcode, opcode.arg0, opcode.arg1, out); \
	}

#include <vm/code/opcodes_list.hpp>


#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
	};

	void writeInstruction(Instruction instruction, std::ostream& out) {
		std::visit(InstructionSerializerVisitor{ out }, instruction);
	}

	class FunctionSerializer final {
		std::ostream&   out;
		const Function& function;
		i64             current_indentation = 0;

		void withIdentWriteLine(const std::function<void(std::ostream&)>& write) const {
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
			writeOption("local_size", function.local_stack_size);
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

	class TypeSerializer final {
		std::ostream&     out;
		const TypeOfData& type;

		struct TypeSerializerVisitor {
			std::ostream& out;

			void operator()(const PrimitiveType& type) const {
				out << "type primitive: ";
				out << type.name.strView() << " ";
				out << type.size;
			}

			void operator()(const PointerType&) const {
				throw base::NotYetImplemented("PointerType serialization");
			}

			void operator()(const StaticTableType& type) const {
				out << "type static_table: ";
				out << type.name.strView() << " ";
				out << type.inner.strView() << " ";
				out << type.table_size;
			}

			void operator()(const DynamicTableType&) const {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const DataType&) const {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const VariantType&) const {
				throw base::NotYetImplemented("DynamicTableType serialization");
			}

			void operator()(const FunctionType& fun) const {
				// type fun: main {} int64
				out << "type fun: ";
				out << fun.name.strView() << " {";
				bool first = true;
				for (const auto& param: fun.parameters) {
					if (first)
						out << " ";
					else
						out << ",";
					out << param.strView() << " ";
					first = false;
				}
				out << "} " << fun.result.strView();
			}
		};

	public:
		TypeSerializer(std::ostream& out, const TypeOfData& type): out(out), type(type) {}

		void write() const { std::visit(TypeSerializerVisitor{ out }, type); }
	};

	void serialize(const Function& function, std::ostream& out) {
		FunctionSerializer serializer(out, function);
		serializer.write();
		out << '\n';
	}

	void serialize(const TypeOfData& type, std::ostream& out) {
		TypeSerializer serializer(out, type);
		serializer.write();
		out << '\n';
	}

}
