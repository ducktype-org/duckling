// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../access/access_pst.hpp"

#include <any>
#include <variant>

namespace pst {
	/**
	 * @brief Additional data optionally stored in root elements of the PST.
	 * @TODO: #2397 probably move or remove it.
	 */
	struct AdditionalRootData final {
		struct MacroExpansionParent final {
			/**
			 * @brief Optional source element of macro expansion the PST was generated from.
			 */
			AccessLocked<LangElement> expand_element;
		};

		struct ModuleParent final {
			/**
			 * @brief frontend::ModuleID the PST was generated from.
			 */
			std::any module_id;
		};

		struct BakedTemplateParent final {
			std::any template_bake_data;
		};

		/**
		 * @brief Source of PST.
		 * @note This is used mostly for determining the parent helios-scope of PST root elements.
		 */
		std::variant<MacroExpansionParent, ModuleParent, BakedTemplateParent> pst_parent;

		template<class T>
		[[nodiscard]]
		const T& getAs() const {
			return std::get<T>(pst_parent);
		}
	};
}
