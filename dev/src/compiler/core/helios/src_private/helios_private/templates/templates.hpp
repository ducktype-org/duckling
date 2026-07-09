#pragma once

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::templates {

	// This might be useful in the future.
	// enum class TemplateKind {
	//     Function,
	//     Class,
	//     Namespace,
	//     Const,
	// };


	/**
	 * @brief Key for baking a template symbol ID.
	 * @note Everything related to handling named parameters, template overloading, default template
	 * arguments, implicit coercions of arguments, etc. should be handled by the caller using this
	 * key. Queries using this key assumes perfect match of template arguments to template
	 * parameters.
	 */
	struct TemplateBakeKey final {
		/**
		 * @brief The symbol ID of the template to bake.
		 */
		SymID template_sym_id;

		/**
		 * Arguments that should 1-1 match the template parameters of the template symbol ID.
		 * This is used to generate constant symbols for the template arguments, and link them to
		 * the given baked template.
		 */
		std::vector<ctv::CompileTimeValue> template_arguments;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Data linked to the baked template, produced by baking a template symbol ID.
	 * This will be passed to PST root as additional root data in type-opaque way, so other helios
	 * code can retrieve it when needed.
	 *
	 * @TODO: #3071 link this to the proper custom root element, liked pst::BakedTemplateRoot or
	 * something like that.
	 */
	struct TemplateBakePSTLinkedData final {
		pst::AccessLocked<pst::LangElement> pst_parent_element;

		// move the deleter to cpp now?
		struct TemplateArgumentsSymbolsDeleter final {
			void del(std::atomic<std::vector<SymID>*>* ptr);
		};

		/**
		 * @brief The symbols for the template arguments.
		 *
		 * @TODO: Implementation of this is a bit hacky, try to improve this.
		 *
		 * @note: We use custom deleter to delete the vector, because we can't use unique_ptr inside
		 * atomic.
		 */
		mutable SharedBox<std::atomic<std::vector<SymID>*>>
			template_arguments_symbols;
	};

	/**
	 * @brief Query to bake a template symbol ID.
	 * @important: Implementation of this query is very fragile for now.
	 *
	 * @param key The template bake key.
	 * @return The baked symbol ID.
	 */
	DECLARE_QUERY(QueryBakeTemplateSymID, TemplateBakeKey, query::QResult<SymID>, ({}));

	// @NOTE: if we will have bale to hout unit, it should first bake everything to the sym id, and
	// then gather the content of the HOUTUnit
}
