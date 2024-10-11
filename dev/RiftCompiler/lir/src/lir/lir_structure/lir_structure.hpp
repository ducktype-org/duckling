#pragma once

#include <typesystem/lower/type_layout.hpp>
#include <base/stable_container.hpp>
#include <base/stringifyable_enum.hpp>

namespace compiler::mir {
	struct MirLocal;
}



// clang-format off

// @TODO: two enums with the same name included in one cpp lead to compiler error.
// fix it

MAKE_STRINGIFYABLE_ENUM(compiler::lir, u64, LirOperation,
	Uninitialized, //< placeholder for uninitialized value, should not be in LIR output

	Assign, //< simple byte by byte assignment

	/**
	 * @brief Placeholder. 
	 * @todo  Some decisions here to be made about operations like that.
     * Perhaps we want more generic code for MIR, so algorithms are simpler.
	 * There could be single operation for all Add, Sub, etc, and single one for all
	 * comparisons.
	 */
	IntegerAdd,
	
	ReturnVoid,
	ReturnValue,
	Jump,
	Branch
);

// clang-format on


namespace compiler::lir {
	struct LirLocal;

	/**
	 * @brief Reference to local variable in LIR.
	 */
	using LocalRef = CRef<LirLocal>;

	struct MirLocation {
		// ...
	};

	/**
	 * @brief Description of a LIR Local variable or function argument.
	 * @note This structure should only be stored directly in LIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct LirLocal final {
		/**
		 * @brief HELIOS id of the variable.
		 * @note This is a temporary solution.
		 */
		helios::SymID   helios_id;

		// a copy of type-layout here might bu sub-optimal
		tsl::TypeLayout type;

	private:
		LirLocal(helios::SymID helios_id, tsl::TypeLayout type):
			helios_id(helios_id),
			type(std::move(type)) {}

		static LirLocal fromMir(query::Context& ctx, const mir::MirLocal& mir_local);

		friend struct Function;
		friend LocalRef;
	};

	struct Instruction final {
		LirOperation operation = LirOperation::Uninitialized;
		base::Optional<LocalRef> output;
		std::vector<MirLocation> arguments;

		// @TODO: each Instruction should have source position reference

		Instruction()                   = delete;
		Instruction(const Instruction&) = default;
		Instruction(Instruction&&)      = default;

		Instruction(
			LirOperation                  operation,
			base::Optional<LocalRef>   output,
			std::vector<MirLocation>   arguments
		):
			operation(operation),
			output(output),
			arguments(std::move(arguments)) {}

		void debugPrint(std::ostream& output) const;
	};

	struct Block final {
		std::vector<Instruction> instructions;
		Instruction              terminator;
	};


	/**
	 * @brief Function in LIR.
	 */
	struct Function final {
		// @TODO: is this name mangled somehow:?
		base::StrID                  name;

		std::vector<Block>           blocks;
		
		base::StableVector<LirLocal> local_list;

		void debugPrint(std::ostream& output) const;

	};

}
