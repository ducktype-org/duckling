/**
 * @brief This file contains structures representing VM instructions.
 * Each opcode has a corresponding structure with name `Op_{opcode_name}`.
 * A variant structure that can store any instruction is called `Instruction`.
 */

#pragma once

#include "element_base.hpp"

#include <base/pointers/box.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <variant>

#define VM_INSTR_FROM_NAME(opcode) vm::code::instructions::Op_##opcode

namespace vm::code {
	/**
	 * @brief `instructions` namespace encapsulates available VM instructions.
	 */
	namespace instructions {
#define DECLARE_0ARGS() \
	auto argsAsTuple(this auto&&) { return std::forward_as_tuple(); }
#define DECLARE_1ARGS(ARG0)                                          \
	ARG0 arg0;                                                       \
	template<typename Self>                                          \
	auto argsAsTuple(this Self&& self) {                             \
		return std::forward_as_tuple(std::forward<Self>(self).arg0); \
	}
#define DECLARE_2ARGS(ARG0, ARG1)                                                                   \
	ARG0 arg0;                                                                                      \
	ARG1 arg1;                                                                                      \
	template<typename Self>                                                                         \
	auto argsAsTuple(this Self&& self) {                                                            \
		return std::forward_as_tuple(std::forward<Self>(self).arg0, std::forward<Self>(self).arg1); \
	}
#define DECLARE_3ARGS(ARG0, ARG1, ARG2)    \
	ARG0 arg0;                             \
	ARG1 arg1;                             \
	ARG2 arg2;                             \
	template<typename Self>                \
	auto argsAsTuple(this Self&& self) {   \
		return std::forward_as_tuple(      \
			std::forward<Self>(self).arg0, \
			std::forward<Self>(self).arg1, \
			std::forward<Self>(self).arg2  \
		);                                 \
	}
#define DECLARE_4ARGS(ARG0, ARG1, ARG2, ARG3) \
	ARG0 arg0;                                \
	ARG1 arg1;                                \
	ARG2 arg2;                                \
	ARG3 arg3;                                \
	template<typename Self>                   \
	auto argsAsTuple(this Self&& self) {      \
		return std::forward_as_tuple(         \
			std::forward<Self>(self).arg0,    \
			std::forward<Self>(self).arg1,    \
			std::forward<Self>(self).arg2,    \
			std::forward<Self>(self).arg3     \
		);                                    \
	}
#define GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define DECLARE_ARGS(...)                         \
	GET_MACRO(                                    \
		__VA_ARGS__ __VA_OPT__(, ) DECLARE_4ARGS, \
		DECLARE_3ARGS,                            \
		DECLARE_2ARGS,                            \
		DECLARE_1ARGS,                            \
		DECLARE_0ARGS                             \
	)                                             \
	(__VA_ARGS__)

		namespace detail {
			template<typename T>
			struct InstructionArgs;
		}

#define HANDLE_INSTR_ARGS(INSTR, ...)                                                         \
	struct Op_##INSTR;                                                                        \
	namespace detail {                                                                        \
		template<>                                                                            \
		struct InstructionArgs<Op_##INSTR> {                                                  \
			DECLARE_ARGS(__VA_ARGS__)                                                         \
			constexpr bool operator==(const InstructionArgs<Op_##INSTR>&) const = default;    \
		};                                                                                    \
	}                                                                                         \
	struct Op_##INSTR final: ElementBase, detail::InstructionArgs<Op_##INSTR> {               \
		using ArgTypes                         = std::tuple<__VA_ARGS__>;                     \
		using ArgsWrapperType                  = detail::InstructionArgs<Op_##INSTR>;         \
		constexpr static std::string_view NAME = #INSTR;                                      \
		using ArgsWrapperType::argsAsTuple;                                                   \
		template<typename... Args>                                                            \
		requires std::is_constructible_v<ArgsWrapperType, Args...>                            \
		      && (sizeof...(Args) == std::tuple_size_v<ArgTypes>) Op_##INSTR(Args&&... args): \
			  ArgsWrapperType{ std::forward<Args>(args)... } {}                               \
		constexpr bool operator==(const Op_##INSTR& other) const noexcept {                   \
			return static_cast<const ArgsWrapperType&>(*this)                                 \
			    == static_cast<const ArgsWrapperType&>(other);                                \
		}                                                                                     \
	};

#include <vm/bytecode/instruction_definitions.hpp>

#undef DECLARE_0ARGS
#undef DECLARE_1ARGS
#undef DECLARE_2ARGS
#undef DECLARE_3ARGS
#undef DECLARE_4ARGS
#undef GET_MACRO
#undef DECLARE_ARGS
#undef HANDLE_INSTR_ARGS

		struct Comment;

		namespace detail {
			template<>
			struct InstructionArgs<Comment> {
				auto argsAsTuple(this auto&&) { return std::forward_as_tuple(); }

				constexpr bool operator==(const InstructionArgs<Comment>&) const = default;
			};
		}

		/**
		 * @brief An extra instruction that represents a comment.
		 * @note It also helps with macro, because without it the template
		 * below would finish with a `,`, which does not compile.
		 */
		struct Comment final: ElementBase, detail::InstructionArgs<Comment> {
			Comment()                              = default;
			using ArgTypes                         = std::tuple<>;
			using ArgsWrapperType                  = detail::InstructionArgs<Comment>;
			constexpr static std::string_view NAME = "Comment";
			using ArgsWrapperType::argsAsTuple;

			Comment(base::StrID comment): comment(comment) {}

			base::StrID comment;

			constexpr bool operator==(const Comment& other) const noexcept {
				return comment == other.comment;
			}
		};
	}

	using Instruction = std::variant<
#define HANDLE_INSTR(opcode) VM_INSTR_FROM_NAME(opcode),
#include <vm/bytecode/instruction_definitions.hpp>
#undef HANDLE_INSTR
		instructions::Comment>;

	template<typename T>
	concept IsInstruction = base::IS_VARIANT_MEMBER_V<std::remove_cvref_t<T>, Instruction>;

	template<typename T>
	concept TwoArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 2;

	template<typename T>
	concept OneArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 1;

	template<typename T>
	concept ZeroArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 0;
}
