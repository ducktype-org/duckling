#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>

namespace compiler::helios {

	/**
	 * Helper struct to represent the result of getPSTElementParent.
	 *
	 * @note If we ever need to return more information about the parent, feel free to extend this
	 * struct.
	 */
	struct PSTParentResult final {
		std::variant<pst::AccessLocked<pst::LangElement>, frontend::ModuleID> state;

        [[nodiscard]]
        bool isLangElement() const { return std::holds_alternative<pst::AccessLocked<pst::LangElement>>(state); }

		[[nodiscard]]
		pst::AccessLocked<pst::LangElement> getAsLangElement() const {
			if (std::holds_alternative<pst::AccessLocked<pst::LangElement>>(state))
				return std::get<pst::AccessLocked<pst::LangElement>>(state);
			else
				CORE_PANIC("PSTParentResult does not hold a LangElement");
		}

		[[nodiscard]]
		frontend::ModuleID getAsModuleID() const {
			if (std::holds_alternative<frontend::ModuleID>(state))
				return std::get<frontend::ModuleID>(state);
			else
				CORE_PANIC("PSTParentResult does not hold a ModuleID");
		}
	};

	/**
	 * Macro+frontend aware operation to obtain the parent of a PST element.
	 * It does the following:
	 * 1) If the element has a parent in the immediate PST structure (i.e. is not a PST root
	 * element), return it. 2) If the element is a root element of a PST that was created by a macro
	 * expansion, return the expand element that created it. 3) If the element is a root element of
	 * a PST that was created by a module, returns the module id.
	 *
	 * @note This is a common operation that is used in multiple places in the compiler, and it has
	 * some non-trivial logic to handle macro expansions and modules, so we provide this helper
	 * function to unify this logic in one place.
     *
     * @TODO: #2407 use AccessMaybeLocked here? I left Access for now, since this function always has to unlock the element anyway.
	 */
	PSTParentResult getPSTElementParent(query::Context& ctx, pst::Access<pst::LangElement> element);

}
