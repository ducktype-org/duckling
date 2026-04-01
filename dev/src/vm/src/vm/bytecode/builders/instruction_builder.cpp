#include "instruction_builder.hpp"

#include <base/except/exceptions.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>

#include <iostream>
#include <sstream>
#include <unordered_map>

namespace vm::code::builders {
	namespace {
		/**
		 * @brief Appends opcode argument as string to a to a stream.
		 */
		void pushOpcodeArg(const opargs::OpCodeArg& arg, std::ostream& out) {
			out << std::visit([]<class T>(const T&) { return T::OP_SHORT; }, arg);
		}

		/**
		 * @brief Appends opcode kind as string to a to a stream.
		 */
		void pushOpcodeKind(const OpKind& kind, std::stringstream& out) {
			out << base::enumToStr(kind);
		}

		/// Helper for makeInstructionFromArgs
		template<vm::code::IsInstruction I>
		vm::code::Instruction fromArgs(const std::vector<opargs::OpCodeArg>& args) {
			using ArgTypes        = I::ArgTypes;
			constexpr usize ARITY = std::tuple_size_v<ArgTypes>;

			CORE_ASSERT(
				args.size() == ARITY, base::strConcat("Invalid number of args for ", I::NAME)
			);

			return [&]<usize... Indices>(std::index_sequence<Indices...>) {
				bool all_ok
					= (std::holds_alternative<std::tuple_element_t<Indices, ArgTypes>>(
						   args.at(Indices)
					   )
				       && ...);
				CORE_ASSERT(all_ok, base::strConcat("Invalid argument types for ", I::NAME));

				return I{ std::get<std::tuple_element_t<Indices, ArgTypes>>(args.at(Indices))... };
			}(std::make_index_sequence<ARITY>{});
		}
	}

	vm::code::Instruction makeInstructionFromArgs(
		base::StrID name, const std::vector<opargs::OpCodeArg>& args
	) {
		std::cout << args.size() << '\n';

#define HANDLE_INSTR(opcode) \
	std::make_pair(base::StrID(#opcode), fromArgs<VM_INSTR_FROM_NAME(opcode)>),
		static std::unordered_map name_to_factory{
#include <vm/bytecode/instruction_definitions.hpp>
		};
#undef HANDLE_INSTR

		CORE_ASSERT(
			name_to_factory.contains(name), base::strConcat("Instruction ", name, " does not exist")
		);

		return name_to_factory.at(name)(args);
	}
}

void vm::code::builders::InstructionBuilder::pushArg(const vm::opargs::OpCodeArg& arg) {
	args.push_back(arg);
}

vm::code::builders::InstructionBuilder::InstructionBuilder(OpKind kind) { setKind(kind); }

void vm::code::builders::InstructionBuilder::setKind(OpKind kind) {
	this->kind = kind;
	kind_set   = true;
}

vm::code::Instruction vm::code::builders::InstructionBuilder::build() const {
	CORE_ASSERT(kind_set, "InstructionBuilder::build: kind_set = false");

	usize             arg_count = args.size();
	std::stringstream name_stream;
	pushOpcodeKind(kind, name_stream);
	for (usize i = 0; i < arg_count; i++) {
		name_stream << "_";
		pushOpcodeArg(args[i], name_stream);
	}
	auto instr_name = base::StrID{ name_stream.str().c_str() };

	return makeInstructionFromArgs(instr_name, args);
}
