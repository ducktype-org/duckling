/**
 * @brief This file contains structures representing VM instructions.
 * Each instruction has a corresponding structure with name `Op_{instruction_name}`.
 * `Instruction` is a handmade variant of sorts grouping all concrete `Op_{foo}` instructions.
 * Not using `std::variant` is a very deliberate choice, as when working with 200+ alternatives
 * compilation times and artifact sizes get very unpleasant.
 * (For connoisseurs: getting to the n-th alternative of a std::variant
 * is fast at runtime, but at compile time requires instantiating O(n) templates.
 * Furhtermore, each alternative does not share the template instances with the other alternatives
 * resulting in a quadratic number of templates getting instantiated when e.g. visiting a variant.)
 *
 * This code uses a lot of X-macros, making it somewhat unwieldy, but strives to provide a usable
 * interface so that the users of `Instruction` can hopefully be macro-free. In particular it offers:
 * - `Instruction::kind() -> InstructionKind`
 * - `Instruction::name() -> StrID`
 * - `Instruction::get<ConcreteInstructionType>() -> ConcreteInstructionType`
 * - `Instruction::getMaybe<ConcreteInstructionType>() -> Optional<ConcreteInstructionType>`
 * - `Instruction::visit(CallableAcceptingEachConcreteInstruction) -> ResultOfSaidCallable`
 * - `instr_match` macro anologous to `variant_match`
 * - `args() -> std::vector<opargs::OpCodeArgCRef>` helpful for generic operations on args
 *   (like serialisation) with runtime dispatch for faster compilation and less template mess
 */

#pragma once

#include "element_base.hpp"

#include <base/pointers/box.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>

// Useful for turning a name to a properly qualified type name in X-macros.
#define VM_INSTR_FROM_NAME(name)      vm::code::instructions::Op_##name
#define VM_INSTR_KIND_FROM_NAME(name) vm::code::InstructionKind::Op_##name

namespace vm::code {

	enum class InstructionKind : u64 {
#define HANDLE_INSTR(name) Op_##name,
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
		Comment
	};

	namespace detail {
		// This structure allows us to easily define the `IsInstruction` concept.
		struct InstructionBase: ElementBase {};

		// Helper useful for getting rid of the leading comma resulting from `FOR_EACH`.
		template<typename THead, typename... TTail>
		using TailTuple = std::tuple<TTail...>;
	}

	template<typename T>
	concept IsInstruction = std::derived_from<T, detail::InstructionBase>;

	/**
	 * @brief `instructions` namespace encapsulates available VM instructions.
	 */
	namespace instructions {


// Macros used as arguments for `FOR_EACH` throughout the definition of the instructions.
#define ARG_DECLARE(type, name) type name;
#define ARG_COMPARE(type, name) name == other.name&&
#define ARG_TYPE(type, name)    type
#define ARG_NAME(type, name)    name
#define ARG_PARAM(type, name)   type name,
#define ARG_INIT_LIST(type, name) \
	, name { std::move(name) }
#define ARG_ALIAS(instr_name, arg_type_name) &alts.op_##instr_name.ARG_NAME arg_type_name,
#define ARG_TYPE_LIST(type, name)            , type

		// Concrete instruction type
#define HANDLE_INSTR_ARGS(name, ...)                                                                \
	struct Op_##name final: detail::InstructionBase {                                               \
		/* Aliases useful in templates */                                                           \
		constexpr static std::string_view NAME = #name;                                             \
		constexpr static InstructionKind  KIND = VM_INSTR_KIND_FROM_NAME(name);                     \
		using ArgTypes = detail::TailTuple<void FOR_EACH(ARG_TYPE_LIST EXPAND, __VA_ARGS__)>;       \
                                                                                                    \
		/* Each argument is held directly as a member and is named as in the definition file */     \
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
		 * @note It also helps with macros, as it often goes after an othewise trailing comma.
		 * It's treated seperately and while useful for debugging
		 * the compiler backend, the parser does not generate them.
		 */
		struct Comment final: detail::InstructionBase {
			Comment()                              = default;
			constexpr static std::string_view NAME = "Comment";
			constexpr static InstructionKind  KIND = InstructionKind::Comment;
			using ArgTypes                         = std::tuple<>;

			Comment(base::StrID comment): comment(comment) {}

			base::StrID comment;

			constexpr bool operator==(const Comment& other) const noexcept {
				return comment == other.comment;
			}
		};
	}

	namespace internal {
		// Plain unions don't do anything clever when copying/moving,
		// therefore unless we want to write a lot of boilerplate, we have to make sure that all
		// concrete instructions can me copied using a simple memcopy.
		// This is a good property for an instruction to have anyways, as the VM will have to handle
		// a lot of instructions and we don't want custom operations to slow it down.
		template<typename T>
		concept VeryTrivial
			= std::is_trivially_copy_constructible_v<T> && std::is_trivially_move_constructible_v<T>
		   && std::is_trivially_copy_assignable_v<T> && std::is_trivially_move_assignable_v<T>;
	}

	/// Handmade variant of all concrete instructions with helper accessors.
	// Be careful when editing: notice that many things have to be seperately defined
	// for `Comment` as it's not an instruction defined in the definition file.
	class Instruction final {
		// Sanity check for better errors.
#define HANDLE_INSTR(name) static_assert(internal::VeryTrivial<VM_INSTR_FROM_NAME(name)>);
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
		static_assert(internal::VeryTrivial<instructions::Comment>);

	public:
		Instruction()                              = delete;
		Instruction(const Instruction&)            = default;
		Instruction(Instruction&&)                 = default;
		Instruction& operator=(const Instruction&) = default;
		Instruction& operator=(Instruction&&)      = default;

		// Constructors from concrete instructions
#define HANDLE_INSTR_ARGS(name, ...)                        \
	Instruction(const VM_INSTR_FROM_NAME(name) & concrete): \
		  instr_kind{ VM_INSTR_KIND_FROM_NAME(name) },      \
		  alts{ .op_##name = concrete } {}
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR_ARGS

		Instruction(const instructions::Comment& concrete):
			  instr_kind{ InstructionKind::Comment },
			  alts{ .comment = concrete } {}

		[[nodiscard]] InstructionKind kind() const { return instr_kind; }

		[[nodiscard]] base::StrID name() const {
			static std::array map = {
#define HANDLE_INSTR(name) base::StrID(#name),
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
				base::StrID("[comment]")
			};
			return map.at(std::to_underlying(instr_kind));
		}

		// Specialisations are outside the class, as required by gcc
		template<IsInstruction T>
		[[nodiscard]] T& get();
		template<IsInstruction T>
		[[nodiscard]] const T& get() const;

		template<IsInstruction T>
		[[nodiscard]] base::Optional<Ref<T>> getMaybe() {
			if (instr_kind == T::KIND)
				return &get<T>();
			else
				return std::nullopt;
		}

		template<IsInstruction T>
		[[nodiscard]] base::Optional<CRef<T>> getMaybe() const {
			if (instr_kind == T::KIND)
				return &get<T>();
			else
				return std::nullopt;
		}

		template<typename V>
		decltype(auto) visit(this auto&& self, V&& visitor) {
			switch (self.instr_kind) {
#define HANDLE_INSTR(name)              \
	case VM_INSTR_KIND_FROM_NAME(name): \
		return std::forward<V>(visitor)(self.template get<VM_INSTR_FROM_NAME(name)>());
				break;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
			case InstructionKind::Comment:
				return std::forward<V>(visitor)(self.template get<instructions::Comment>());
				break;
			}
			CORE_UNREACHABLE();
		}

		/// View of instruction arguments for generic operations.
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
			CORE_UNREACHABLE();
		}


	private:
		InstructionKind instr_kind;

		union Alts {
#define HANDLE_INSTR(name) VM_INSTR_FROM_NAME(name) op_##name;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
			instructions::Comment comment;
		} alts;
	};

#define HANDLE_INSTR(name)                                                                     \
	template<>                                                                                 \
		[[nodiscard]] inline const VM_INSTR_FROM_NAME(name) & Instruction::get() const {       \
		CORE_ASSERT(instr_kind == VM_INSTR_KIND_FROM_NAME(name), "Invalid instruction kind."); \
		return alts.op_##name;                                                                 \
	}                                                                                          \
	template<>                                                                                 \
		[[nodiscard]] inline VM_INSTR_FROM_NAME(name) & Instruction::get() {                   \
		CORE_ASSERT(instr_kind == VM_INSTR_KIND_FROM_NAME(name), "Invalid instruction kind."); \
		return alts.op_##name;                                                                 \
	}
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR

	template<>
	[[nodiscard]] inline const instructions::Comment& Instruction::get() const {
		CORE_ASSERT(instr_kind == InstructionKind::Comment, "Invalid instruction kind.");
		return alts.comment;
	}

	template<>
	[[nodiscard]] inline instructions::Comment& Instruction::get() {
		CORE_ASSERT(instr_kind == InstructionKind::Comment, "Invalid instruction kind.");
		return alts.comment;
	}

	constexpr bool operator==(const Instruction& a, const Instruction& b) {
		if (a.kind() != b.kind()) return false;

		switch (a.kind()) {
#define HANDLE_INSTR(name)              \
	case VM_INSTR_KIND_FROM_NAME(name): \
		return a.get<VM_INSTR_FROM_NAME(name)>() == b.get<VM_INSTR_FROM_NAME(name)>();
			break;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
		case InstructionKind::Comment:
			return a.get<instructions::Comment>() == b.get<instructions::Comment>();
			break;
		}
		CORE_UNREACHABLE();
	}

#undef ARG_DECLARE
#undef ARG_COMPARE
#undef ARG_TYPE
#undef ARG_NAME
#undef ARG_PARAM
#undef ARG_INIT_LIST
#undef ARG_ALIAS
#undef ARG_TYPE_LIST
}

// The following macros are almost copied from <base/extend_cpp/variant_match.hpp>.
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
