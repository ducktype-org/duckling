#pragma once

#include <typesystem/lower/type_layout.hpp>
#include <base/stable_container.hpp>

namespace compiler::lir {
	struct LirLocal;

	/**
	 * @brief Reference to local variable in LIR.
	 */
	using LocalRef = CRef<LirLocal>;

	/**
	 * @brief Description of a LIR Local variable or function argument.
	 * @note This structure should only be stored directly in LIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct MirLocal final {
		// for now we just keep HELIOS id as a temporary solution:
		helios::SymID   helios_id;

		tsl::TypeLayout type;

	private:
		MirLocal(helios::SymID helios_id, tsl::TypeLayout type):
			helios_id(helios_id),
			type(type) {}

		friend struct Function;
		friend LocalRef;
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
