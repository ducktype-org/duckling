#include "serializer.hpp"

#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>

#include <lang_definitions/key_spec_op.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/const_value_visitor.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <bit>
#include <iomanip>
#include <ranges>

namespace vm::code {
	std::string toString(opargs::Immediate arg) { return std::to_string(arg.value); }

#define LOCAL_TO_STRING(Tp) \
	std::string toString(vm::opargs::Tp arg) { return arg.var_name.str(); }

	FOR_EACH(LOCAL_TO_STRING, VM_OPARG_PLACE_TYPES);
#undef LOCAL_TO_STRING

	std::string toString(opargs::Type arg) { return arg.type_name.str(); }

	std::string toString(opargs::Field arg) {
		return base::strConcat(arg.type_name, ".", arg.field_name);
	}

	std::string toString(opargs::FunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::BuiltinFunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::ExtCFunctionName arg) { return arg.function_name.str(); }

	std::string toString(opargs::FFIFunctionName arg) { return arg.function_name.str(); }

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

	/**
	 * @brief Writes the annotation of `symbol_name` (if any) as a comment line of its own, so that
	 * it precedes the declaration the caller is about to serialize.
	 */
	void displaySymbolAnnotation(
		const SymbolAnnotator& annotate, const base::StrID symbol_name, std::ostream& out
	) {
		if (!annotate) return;
		const std::string annotation = annotate(symbol_name);
		if (annotation.empty()) return;
		displayComment(annotation, out);
		out << '\n';
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
		std::ostream&          out;
		const Function&        function;
		const SymbolAnnotator& annotate;
		i64                    current_indentation = 0;

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
		FunctionSerializer(
			std::ostream& out, const Function& function, const SymbolAnnotator& annotate
		):
			  out(out),
			  function(function),
			  annotate(annotate) {}

		void display() {
			displaySymbolAnnotation(annotate, function.name.str, out);
			out << "function " << function.name.str.strView() << " { ";
			bool first = true;
			for (const auto& param: function.signature.parameters) {
				if (!first) out << ", ";
				out << param.str.strView();
				first = false;
			}
			out << " } -> { ";
			first = true;
			for (const auto& param: function.signature.result_types) {
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
				out << type.size.asInt();
			}

			void operator()(const PointerType& type) const {
				out << "type pointer: ";
				out << type.name.strView() << " ";
				out << type.inner.strView();
			}

			void operator()(const CPointerType& type) const {
				out << "type cpointer: ";
				out << type.name.strView();
				if (type.inner.has_value()) out << " " << type.inner->strView();
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

			void operator()(const DataType& type) const {
				out << "type data: ";
				out << type.name.strView() << " {\n";
				for (auto field: type.fields)
					out << "    " << field.name.strView() << ": " << field.type.strView() << ",\n";
				out << "}";
				if (type.packed) out << " packed";
				if (type.assert_size.has_value()) out << " assert_size " << *type.assert_size;
				out << "\n";
			}

			void operator()(const VariantType&) const {
				throw base::NotYetImplemented("VariantType serialization");
			}

			void operator()(const FunctionType& fun) const {
				// type fun: main {} int64
				out << "type fun: ";
				out << fun.name.strView() << " { ";
				bool first = true;
				for (const auto& param: fun.parameters) {
					if (!first) out << ", ";
					out << param.strView();
					first = false;
				}
				out << " } -> { ";
				first = true;
				for (const auto& reslts: fun.result) {
					if (!first) out << ", ";
					out << reslts.strView();
					first = false;
				}
				out << " }";
			}

			void operator()(const OpaqueType& type) const {
				out << "type opaque: ";
				out << type.name.strView() << " ";
				out << type.size.asInt();
			}

			void operator()(const ClassType& clazz) const {
				out << "type class:  " << clazz.name.strView() << "{\n";
				out << "    fields: [";
				for (auto field: clazz.fields)
					out << field.name.strView() << ": " << field.type.strView() << ", ";
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

	class ConstValueSerializer final: public code::ConstVisitor {
		std::ostream& out;

		void visitConstantImmediate(const code::ConstantImmediate& val) final {
			static_assert(
				std::endian::native == std::endian::little,
				"Only little-endian platforms are supported"
			);
			// Output as hex literal: 0x followed by exactly (2*size) hex digits.
			// This makes the byte count inferable from the serialized form.
			out << "0x";
			// We save the number in the big endianness.
			for (size_t i = val.size.asInt(); i-- > 0;)
				out << std::format("{:02X}", std::to_integer<unsigned>(val.content.at(i)));
		}

		void visitConstantClass(const code::ConstantClass& val) final {
			out << lang_def::keywordToStr(lang_def::Keyword::BCClass).strView() << " { ";
			bool first = true;
			for (const auto& [name, field_val]: val.fields) {
				if (!first) out << ", ";
				out << name.strView() << ": ";
				field_val->acceptVisitor(*this);
				first = false;
			}
			out << " }";
		}

		void visitConstantFixedSizeTable(const code::ConstantFixedSizeTable& val) final {
			out << lang_def::keywordToStr(lang_def::Keyword::BCFixedSizeTable).strView() << " [ ";
			bool first = true;
			for (const auto& elem: val.elements) {
				if (!first) out << ", ";
				elem->acceptVisitor(*this);
				first = false;
			}
			out << " ]";
		}

	public:
		ConstValueSerializer(std::ostream& out): out(out) {}
	};

	void serializeConstValue(const ConstantValue& const_value, std::ostream& out) {
		out << lang_def::keywordToStr(lang_def::Keyword::BCInitialValue).strView() << ": ";
		ConstValueSerializer serializer(out);
		const_value.data->acceptVisitor(serializer);
	}

	class GlobalDataSerializer final {
		std::ostream&          out;
		const GlobalData&      global_data;
		const SymbolAnnotator& annotate;

	public:
		GlobalDataSerializer(
			std::ostream& out, const GlobalData& global_data, const SymbolAnnotator& annotate
		):
			  out(out),
			  global_data(global_data),
			  annotate(annotate) {}

		void display() {
			displaySymbolAnnotation(annotate, global_data.name.str, out);
			out << lang_def::keywordToStr(lang_def::Keyword::BCGlobalData).strView() << ' ';
			out << global_data.name.str.strView() << " " << global_data.type.str.strView() << " {";
			bool has_content   = false;
			auto maybe_newline = [&] {
				if (has_content) out << ",";
				out << "\n    ";
				has_content = true;
			};

			if (global_data.is_constant) {
				maybe_newline();
				out << lang_def::keywordToStr(lang_def::Keyword::BCIsConstant).strView() << ": ";
				out << lang_def::keywordToStr(lang_def::Keyword::BCTrue).strView();
			}

			if (global_data.initial_value.has_value()) {
				maybe_newline();
				serializeConstValue(global_data.initial_value.value(), out);
			}

			if (global_data.ctor_name.has_value()) {
				maybe_newline();
				out << lang_def::keywordToStr(lang_def::Keyword::BCGlobalConstructor).strView()
					<< ": " << global_data.ctor_name.value().str.strView();
			}
			if (global_data.dtor_name.has_value()) {
				maybe_newline();
				out << lang_def::keywordToStr(lang_def::Keyword::BCGlobalDestructor).strView()
					<< ": " << global_data.dtor_name.value().str.strView();
			}
			out << "\n}";
		}
	};

	void serializeFunction(
		const Function& function, std::ostream& out, const SymbolAnnotator& annotate
	) {
		FunctionSerializer serializer(out, function, annotate);
		serializer.display();
		out << '\n';
	}

	void serializeType(const TypeOfData& type, std::ostream& out, const SymbolAnnotator& annotate) {
		displaySymbolAnnotation(annotate, typeName(type), out);
		TypeSerializer serializer(out, type);
		serializer.display();
		out << '\n';
	}

	void serializeGlobal(
		const GlobalData& global_data, std::ostream& out, const SymbolAnnotator& annotate
	) {
		GlobalDataSerializer serializer(out, global_data, annotate);
		serializer.display();
		out << '\n';
	}

	void serializeCode(
		const CodeCollection& code, std::ostream& out, const SymbolAnnotator& annotate
	) {
		for (const auto& type: code.types) serializeType(type, out, annotate);
		out << '\n';
		for (const auto& global_data: code.global_data) serializeGlobal(global_data, out, annotate);
		out << '\n';
		for (const auto& func: code.functions) serializeFunction(func, out, annotate);
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
