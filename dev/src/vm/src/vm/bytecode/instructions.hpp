/**
 * @brief This file contains structures representing VM instructions.
 * Each instruction has a corresponding structure with name `Op_{instruction_name}`.
 * A variant structure that can store any instruction is called `Instruction`.
 */

#pragma once

#include "element_base.hpp"

#include <base/pointers/box.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>

#define VM_INSTR_FROM_NAME(name)      vm::code::instructions::Op_##name
#define VM_INSTR_KIND_FROM_NAME(name) vm::code::InstructionKind::Op_##name

namespace vm::code {

	enum class InstructionKind {
#define HANDLE_INSTR(name) Op_##name,
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
		Comment
	};

	namespace detail {
		// @TODOB comment, this whole file actually
		struct InstructionBase: ElementBase {};
	}

	template<typename T>
	concept IsInstruction = std::derived_from<T, detail::InstructionBase>;

	/**
	 * @brief `instructions` namespace encapsulates available VM instructions.
	 */
	namespace instructions {


#define ARG_DECLARE(type, name) type name;
#define ARG_COMPARE(type, name) name == other.name&&
#define ARG_TYPE(type, name)    type
#define ARG_NAME(type, name)    name
#define ARG_PARAM(type, name)   type name,
#define ARG_INIT_LIST(type, name) \
	, name { std::move(name) }
#define ARG_ALIAS(instr_name, arg_type_name) &alts.op_##instr_name.ARG_NAME arg_type_name,

#define HANDLE_INSTR_ARGS(name, ...)                                                                \
	struct Op_##name final: detail::InstructionBase {                                               \
		constexpr static std::string_view NAME = #name;                                             \
		constexpr static InstructionKind  KIND = VM_INSTR_KIND_FROM_NAME(name);                     \
                                                                                                    \
		FOR_EACH(ARG_DECLARE EXPAND, __VA_ARGS__)                                                   \
                                                                                                    \
		Op_##name(                                                                                  \
			FOR_EACH(ARG_PARAM EXPAND, __VA_ARGS__)                                                 \
				base::Optional<dia::SourcePosition> bytecode_pos                                    \
			= {}                                                                                    \
		):                                                                                          \
			  detail::InstructionBase{ bytecode_pos } FOR_EACH(ARG_INIT_LIST EXPAND, __VA_ARGS__) { \
		}                                                                                           \
                                                                                                    \
                                                                                                    \
		/* Ignores the position */                                                                  \
		constexpr bool operator==([[maybe_unused]] const Op_##name& other) const noexcept {         \
			/* `true` is here to rid of trailing `&&` */                                            \
			return FOR_EACH(ARG_COMPARE EXPAND, __VA_ARGS__) true;                                  \
		}                                                                                           \
	};

#include <vm/bytecode/instruction_definitions.hpp>

#undef HANDLE_INSTR_ARGS

		/**
		 * @brief An extra instruction that represents a comment.
		 * @note It also helps with macro, because without it the template
		 * below would finish with a `,`, which does not compile.
		 */
		struct Comment final: detail::InstructionBase {
			Comment()                              = default;
			constexpr static std::string_view NAME = "Comment";
			constexpr static InstructionKind  KIND = InstructionKind::Comment;

			Comment(base::StrID comment): comment(comment) {}

			base::StrID comment;

			constexpr bool operator==(const Comment& other) const noexcept {
				return comment == other.comment;
			}
		};
	}

	class Instruction {
	public:
		Instruction()                              = delete;
		Instruction(const Instruction&)            = default;
		Instruction(Instruction&&)                 = delete;
		Instruction& operator=(const Instruction&) = delete;
		Instruction& operator=(Instruction&&)      = delete;

#define HANDLE_INSTR_ARGS(name, ...)                                        \
	template<typename T>                                                    \
	requires std::same_as<std::remove_cvref_t<T>, VM_INSTR_FROM_NAME(name)> \
	Instruction(T&& concrete):                                              \
		  instr_kind{ VM_INSTR_KIND_FROM_NAME(name) },                      \
		  alts{ .op_##name = std::forward<T>(concrete) } {}
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR_ARGS

		template<typename T>
		requires std::same_as<std::remove_cvref_t<T>, instructions::Comment>
		Instruction(T&& concrete):
			  instr_kind{ InstructionKind::Comment },
			  alts{ .comment = std::forward<T>(concrete) } {}

		~Instruction() {
			switch (instr_kind) {
#define HANDLE_INSTR(name)              \
	case VM_INSTR_KIND_FROM_NAME(name): \
		alts.op_##name.~Op_##name();    \
		break;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
			case InstructionKind::Comment:
				alts.comment.~Comment();
				break;
			}
		}

		[[nodiscard]] InstructionKind kind() const { return instr_kind; }

		template<IsInstruction T>
		[[nodiscard]] const T& get() const;

		template<IsInstruction T>
		[[nodiscard]] base::Optional<CRef<T>> getMaybe() const {
			if (instr_kind == T::KIND)
				return &get<T>();
			else
				return std::nullopt;
		}

		template<typename V>
		auto visit(V&& visitor) {
			switch (instr_kind) {
#define HANDLE_INSTR(name)              \
	case VM_INSTR_KIND_FROM_NAME(name): \
		return std::forward<V>(visitor)(get<VM_INSTR_FROM_NAME(name)>());
				break;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
			case InstructionKind::Comment:
				return std::forward<V>(visitor)(get<instructions::Comment>());
				break;
			}
		}

		[[nodiscard]] std::vector<opargs::OpCodeArgCRef> args() const {
			switch (instr_kind) {
#define HANDLE_INSTR_ARGS(name, ...)                           \
	case VM_INSTR_KIND_FROM_NAME(name):                        \
		return { FOR_EACH_ARG(ARG_ALIAS, name, __VA_ARGS__) }; \
		break;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR_ARGS
			case InstructionKind::Comment:
				return {};
				break;
			}
		}


	private:
		InstructionKind instr_kind;

		union Alts {
#define HANDLE_INSTR(name) VM_INSTR_FROM_NAME(name) op_##name;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
			instructions::Comment comment;

			// `Instruction` takes care of destroying the active alternative based on `kind`.
			// Unions by default don't get a destructor when having a nontrivially destructable
			// alternative, this silly definition is needed since `~Instruction` implicitely calls
			// `alts.~Alts()`, so it has to be present.
			~Alts() {}
		} alts;
	};

#define HANDLE_INSTR(name)                                                                     \
	template<>                                                                                 \
		[[nodiscard]] const VM_INSTR_FROM_NAME(name) & Instruction::get() const {              \
		CORE_ASSERT(instr_kind == VM_INSTR_KIND_FROM_NAME(name), "Invalid instruction kind."); \
		return alts.op_##name;                                                                 \
	}
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR

	template<>
	[[nodiscard]] const instructions::Comment& Instruction::get() const {
		CORE_ASSERT(instr_kind == InstructionKind::Comment, "Invalid instruction kind.");
		return alts.comment;
	}

}

#define instr_match(value)                                                              \
	PUSH_DIAGNOSTIC                                                                     \
	NO_SHADOW if (bool instr_match_stop                                                 \
	              = true) for (auto&& internal_value = (value); instr_match_stop;       \
	                           instr_match_stop      = false) switch (internal_value.kind()) \
		POP_DIAGNOSTIC

#define instr_case(type, name)                                                               \
	PUSH_DIAGNOSTIC NO_SHADOW break;                                                         \
	case (type::KIND):                                                                       \
		if (bool instr_case_stop = true)                                                     \
			for ([[maybe_unused]] auto&& name = internal_value.get<type>(); instr_case_stop; \
			     instr_case_stop              = false)                                       \
		POP_DIAGNOSTIC

#define instr_case_novalue(type) \
	break;                       \
	case (type::KIND):           \
		if (true)

#define instr_default \
	break;            \
	default:          \
		if (true)
