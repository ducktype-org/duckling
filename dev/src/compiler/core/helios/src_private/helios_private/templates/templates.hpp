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
		
		struct PostponedData final {
			/**
			 * @brief The baked symbol ID of the template.
			 */
			SymID baked_symbol;
			
			/**
			 * @brief The symbols for the template arguments. 
			 */
			std::vector<SymID> template_arguments_symbols;
		};

		struct PostponedDataDeleter final {
			void del(std::atomic<PostponedData*>* ptr);
		};

		/**
		 * @TODO: #3099 Implementation of this is a bit hacky, as it is set by the bake template
		 * query after the PST is baked and used in other queries already. This could be changed if
		 * template_arguments_symbols / creation of the baked SymID didn't required a scope to be created.
		 *
		 * @note: We use raw pointer + custom deleter in this SharedBox to delete the PostponedData, because we can't use unique_ptr inside
		 * atomic.
		 */
		mutable SharedBox<std::atomic<PostponedData*>> postponed_data;
	};

	/**
	 * @brief Query to bake a template symbol ID.
	 * @important: Implementation of this query is very fragile for now.
	 * It will likely be changed in the future, parts of it might be moved elsewhere, and in general
	 * should be use with care for now.
	 *
	 * @TODO: #3112 some of the logic from this query should probably be moved to a different place.
	 * Feel free to do it.
	 *
	 * @param key The template bake key.
	 * @return The baked symbol ID.
	 */
	DECLARE_QUERY(QueryBakeTemplateSymID, TemplateBakeKey, query::QResult<SymID>, ({}));

	/**
	 * @brief Signature for a template declaration.
	 * @TODO: #3112 unify this structure with the ones used in function call processing
	 */
	struct TemplateDeclarationSignature final {
		struct Parameter final {
			base::StrID       name;
			tsh::SymbolType<> type;

			// Not yet supported:
			// base::Optional<ctv::CompileTimeValue> default_value;
		};

		std::vector<Parameter> parameters;
	};

	query::QResult<TemplateDeclarationSignature> getTemplateDeclarationSignature(
		query::Context& ctx, SymID template_sym_id
	);


	// @NOTE: if we will have to bake templates to hout unit, it should probably first call
	// QueryBakeTemplateSymID and then gather the content of the HOUTUnit based on the baked
	// template symbol ID.
}
