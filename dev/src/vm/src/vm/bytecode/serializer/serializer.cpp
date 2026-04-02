#include "serializer.hpp"

#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>

#include <lang_definitions/key_spec_op.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <iomanip>
#include <ranges>

namespace vm::code {
	std::string toString(opargs::Immediate arg) { return std::to_string(arg.value); }

#define LOCAL_TO_STRING(Tp) \
	std::string toString(vm::opargs::Tp arg) { return arg.var_name.str(); }

	FOR_EACH(LOCAL_TO_STRING, VM_OPARG_LOCAL_TYPES);
#undef LOCAL_TO_STRING

#define GLOBAL_TO_STRING(Tp) \
	std::string toString(vm::opargs::Tp arg) { return arg.global_data_name.str(); }

	FOR_EACH(GLOBAL_TO_STRING, VM_OPARG_GLOBAL_TYPES);

	std::string toString(opargs::Type arg) { return arg.type_name.str(); }

	std::string toString(opargs::Field arg) {
		return base::strConcat(arg.type_name, ".", arg.field_name);
	}

	std::string toString(opargs::FunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::BuiltinFunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::ExtCFunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::MethodName arg) { return arg.method_name.str(); }

	std::string toString(opargs::Label arg) { return arg.label_name.str(); }

	std::string argumentToString(const opargs::OpCodeArg& arg) {
		return VISIT(arg, a, return toString(a));
	}

	std::string argumentToString(opargs::OpCodeArgCRef arg) {
		return VISIT(arg, a, return toString(*a));
	}

	void displayComment(const std::string_view comment_content, std::ostream& out) {
		out << '#' << ' ' << comment_content;
	}

	void displayInstruction(Instruction instruction, std::ostream& out) {
		instr_match(instruction) {
			instr_case(instructions::Comment, comment) {
				displayComment(comment.comment.strView(), out);
			}
			instr_default {
				out << std::setw(22) << std::left << instruction.name().strView();
				auto args = instruction.args();
				if (args.size() > 0) {
					out << " ";
					out << std::setw(8) << std::right << argumentToString(args[0]);
					for (auto arg: args | std::views::drop(1))
						out << ", " << std::setw(8) << std::right << argumentToString(arg);
				}
				out << ';';
			}
		}
	}

	class FunctionSerializer final {
		std::ostream&   out;
		const Function& function;
		i64             current_indentation = 0;

		void withIdentDisplayLine(const std::function<void(std::ostream&)>& display) const {
			out << std::string(base::safeIntConv<size_t>(current_indentation), ' ');
			display(out);
			out << "\n";
		}

		void withIdentDisplayLine(std::string_view str) {
			withIdentDisplayLine([&](std::ostream& out) { out << str; });
		}

		void indentUp() { current_indentation += 4; }

		void indentDown() { current_indentation -= 4; }

		void displayCode() {
			for (const auto& instruction: function.body)
				withIdentDisplayLine([&](std::ostream& out) {
					displayInstruction(instruction, out);
				});
		}

	public:
		FunctionSerializer(std::ostream& out, const Function& function):
			  out(out),
			  function(function) {}

		void display() {
			out << "function " << function.name.str.strView() << " { ";
			bool first = true;
			for (const auto& param: function.signature.parameters) {
				if (!first) out << ", ";
				out << param.str.strView();
				first = false;
			}
			out << " } -> { ";
			first = true;
			for (const auto& param: function.signature.result_type) {
				if (!first) out << ", ";
				out << param.str.strView();
				first = false;
			}
			out << " } {\n";

			indentUp();
			displayCode();
			indentDown();
			out << "}\n";
		}
	};

	class TypeSerializer final {
		std::ostream&     out;
		const TypeOfData& type;

		struct TypeSerializerVisitor final {
			std::ostream& out;

			void operator()(const PrimitiveType& type) const {
				out << "type primitive: ";
				out << type.name.strView() << " ";
				out << type.size;
			}

			void operator()(const PointerType& type) const {
				out << "type pointer: ";
				out << type.name.strView() << " ";
				out << type.inner.strView();
			}

			void operator()(const FixedSizeTableType& type) const {
				out << "type fixed_size_table: ";
				out << type.name.strView() << " ";
				out << type.inner.strView() << " ";
				out << type.table_size;
			}

			void operator()(const DynamicTableType& type) const {
				out << "type dynamic_table: ";
				out << type.name.strView() << " ";
				out << type.inner.strView();
			}

			void operator()(const DataType&) const {
				throw base::NotYetImplemented("DataType serialization");
			}

			void operator()(const VariantType&) const {
				throw base::NotYetImplemented("VariantType serialization");
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
						out << ", ";
					out << param.strView();
					first = false;
				}
				out << " } -> { ";
				first = true;
				for (const auto& reslts: fun.result) {
					if (first)
						out << " ";
					else
						out << ", ";
					out << reslts.strView();
					first = false;
				}
				out << " }";
			}

			void operator()(const OpaqueType& type) const {
				out << "type opaque: ";
				out << type.name.strView() << " ";
				out << type.size;
			}

			void operator()(const ClassType& clazz) const {
				out << "type class:  " << clazz.name.strView() << "{\n";
				out << "    fields: [";
				for (auto field: clazz.fields)
					out << field.name.strView() << ": " << field.name.strView() << ", ";
				out << "]\n";
				out << "    abstract: " << clazz.is_abstract << ";\n";
				if (clazz.extends.has_value())
					out << "    extends: " << clazz.extends.value().strView() << ";\n";
				out << "    implements: [";
				for (auto& iface: clazz.implements) out << iface.strView() << ", ";
				out << "]\n";
				out << "    virtual_methods: [";
				for (auto method: clazz.virtual_methods)
					out << method.name.strView() << ": " << method.name.strView() << ", ";
				out << "]\n";
				out << "}";
			}

			void operator()(const InterfaceType& interface) const {
				out << "type interface:  " << interface.name.strView() << "{\n";
				out << "    implements: [";
				for (auto& iface: interface.implements) out << iface.strView() << ", ";
				out << "]\n";
				out << "    virtual_methods: [";
				for (auto method: interface.virtual_methods)
					out << method.name.strView() << ": " << method.name.strView() << ", ";
				out << "]\n";
				out << "}";
			}
		};

	public:
		TypeSerializer(std::ostream& out, const TypeOfData& type): out(out), type(type) {}

		void display() const { std::visit(TypeSerializerVisitor{ out }, type); }
	};

	class GlobalDataSerializer final {
		std::ostream&     out;
		const GlobalData& global_data;

	public:
		GlobalDataSerializer(std::ostream& out, const GlobalData& global_data):
			  out(out),
			  global_data(global_data) {}

		void display() {
			out << lang_def::keywordToStr(lang_def::Keyword::BCGlobalData).strView() << ' ';
			out << global_data.name.str.strView() << " " << global_data.type.str.strView() << " {";
			if (global_data.ctor_name.has_value()) {
				out << "\n    "
					<< lang_def::keywordToStr(lang_def::Keyword::BCGlobalConstructor).strView()
					<< ": " << global_data.ctor_name.value().str.strView() << ",\n";
			}
			if (global_data.dtor_name.has_value()) {
				out << "\n    "
					<< lang_def::keywordToStr(lang_def::Keyword::BCGlobalDestructor).strView()
					<< ": " << global_data.dtor_name.value().str.strView() << ",\n";
			}
			out << '}' << lang_def::specialToStr(lang_def::Special::Semicolon).strView();
		}
	};

	void serializeFunction(const Function& function, std::ostream& out) {
		FunctionSerializer serializer(out, function);
		serializer.display();
		out << '\n';
	}

	void serializeType(const TypeOfData& type, std::ostream& out) {
		TypeSerializer serializer(out, type);
		serializer.display();
		out << '\n';
	}

	void serializeGlobal(const GlobalData& global_data, std::ostream& out) {
		GlobalDataSerializer serializer(out, global_data);
		serializer.display();
		out << '\n';
	}

	void serializeCode(const CodeCollection& code, std::ostream& out) {
		for (const auto& type: code.types) serializeType(type, out);
		out << '\n';
		for (const auto& global_data: code.global_data) serializeGlobal(global_data, out);
		out << '\n';
		for (const auto& func: code.functions) serializeFunction(func, out);
		out << '\n';
	}

	std::string instructionToString(const Instruction& instruction) {
		std::stringstream ss;
		displayInstruction(instruction, ss);
		return ss.str();
	}

	std::string typeToString(const TypeOfData& type) {
		std::stringstream ss;
		TypeSerializer    serializer(ss, type);
		serializer.display();
		return ss.str();
	}
}
