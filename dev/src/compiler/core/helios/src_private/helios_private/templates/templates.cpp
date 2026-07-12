#include "templates.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>  // PR relax it?
#include <frontend/pst_parser/elements/hierarchy/declarations/template_stmt.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>                // PR relax it?
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>  //PR relax it?
#include <frontend/pst_parser/lang_parser_context.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/deductions.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::templates {

	base::Bit256 TemplateBakeKey::queryUnstablePerfectHash() const {
		hashing::SHA256 hasher;
		hashing::addToHash(hasher, template_sym_id.queryUnstablePerfectHash());
		hashing::addToHash(hasher, template_arguments.size());

		for (const auto& arg: template_arguments)
			hashing::addToHash(hasher, arg.queryUnstablePerfectHash());

		return hasher.finalize();
	}

	query::QResult<TemplateDeclarationSignature> getTemplateDeclarationSignature(
		query::Context& ctx, SymID template_sym_id
	) {
		CORE_ASSERT(kind(template_sym_id) == SymbolKind::Template, "SymID is not a Template");

		auto pst_statement      = stmt(ctx, template_sym_id).value();
		auto template_statement = pst_statement.dynamicCast<pst::TemplateStmt>().value();

		auto template_params
			= template_statement->getTemplateDecl().unlock(ctx)->getParams().unlock(ctx);

		TemplateDeclarationSignature output;

		for (const auto& param: *template_params) {
			auto param_unlocked = param.unlock(ctx);

			auto name = param_unlocked->getName().unlock(ctx)->unwrap();
			
			auto type_expression = param_unlocked->getType();
			auto type_ctv
				= getTypeCTVFromPST(ctx, type_expression.unlock(ctx)->getExpr());
			if (type_ctv.hasFailed()) return query::Failed();

			auto type = type_ctv.valueOrPanic().get<tsh::SymbolType<>>().value();

			if (param_unlocked->getValue().has_value()) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Default values for template parameters are not yet supported",
					param_unlocked->getStablePosition()
				));
				return query::Failed();
			}

			output.parameters.emplace_back(TemplateDeclarationSignature::Parameter{
				.name = name,
				.type = type,
			});
		}

		return output;
	}

	struct TemplateBakeStorage final {
		SymID baked_template_sym_id;

		// TODO: add custom pst element todo issue (link root pst data one)
		pst::PST<pst::TemplateStmt> baked_template_pst;
	};

	void TemplateBakePSTLinkedData::TemplateArgumentsSymbolsDeleter::del(
		std::atomic<std::vector<SymID>*>* ptr
	) {
		// Here we delete both raw pointer allocated with new and the atomic wrapper around it.
		auto vec_ptr = ptr->load(std::memory_order_acquire);
		if (vec_ptr) delete vec_ptr;
		delete ptr;
	}

	struct IMPLEMENT_QUERY(QueryBakeTemplateSymID, query::QResult<TemplateBakeStorage>) {

		static std::vector<SymID> bakeTemplateArgumentsSymbols(
			Context&                    ctx,
			TemplateDeclarationSignature signature,
			ScopeID                     scope,
			const QKey&                 q_key
		) {
			std::vector<SymID> symbols;

			u64 i = 0;
			for (const auto& param: signature.parameters) {
				auto ctv = q_key.template_arguments.at(i);
				i++;

				CORE_ASSERT(ctv.getTypeOfStoredValue(ctx) == param.type, "Template argument type does not match template parameter type");

				auto const_symbol
					= ctx.query<defgen::QueryGeneratedSymbol>(defgen::KeyFor_QueryGeneratedSymbol{
						.name = param.name,
						.generated_symbol_data
						= defgen::GeneratedConstant{ ctv, scope },
					});

				symbols.push_back(const_symbol);
			}

			return symbols;
		}

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// most heavy lifting will happen here, and in usage of pst root data

			// outline
			// 1. Assert template symbol ID is valid and is indeed a template
			// 2. Assert template arguments are valid for the template (count, types, etc.)
			// 3. prepare TemplateInstantiationData
			// 4. generate PST-copy with clone machinery and custom data for template arguments
			//    this design is not perfect, but it will fly for now, we will have to change it
			//    once we remove root_data in favor of proper PST nodes with data, but it will
			//    require more work, so let's do it later
			// 5. generate symbol ID for the baked template, and return it, linking the new pst-root
			// as its pst-data
			//
			// NOTE: we will likely not generate template parameter constants here,
			// but rather in query scopes. Although, we could generate them here and link them to
			// the baked symbol ID / TemplateInstantiationData
			//
			// NOTE: it might be good to put TemplateInstantiationData in symbol data,
			// and put just SymID in PST root data

			CORE_ASSERT(
				kind(key.template_sym_id) == SymbolKind::Template, "SymID is not a Template"
			);

			auto signature_qr = getTemplateDeclarationSignature(ctx, key.template_sym_id);
			if (signature_qr.hasFailed()) return query::Failed();
			const auto& signature = signature_qr.valueOrPanic();


			auto pst_statement      = stmt(ctx, key.template_sym_id).value();
			auto template_statement = pst_statement.dynamicCast<pst::TemplateStmt>().value();

			// This is not an error (but could be for UX sake), as this should be catch at the same
			// level as function overloads:!!
			CORE_ASSERT(
				signature.parameters.size() == key.template_arguments.size(),
				"Template arguments count does not match template parameters count"
			);


			// @TODO: #3110 see if this is ok:
			hashing::ComponentHash hash_ctx
				= hashing::ComponentHash(base::StrID(key.queryUnstablePerfectHash().toStringHex()));


			auto cloned
				= template_statement->clone().dynamicCast<pst::TemplateStmt>().toOptBox().value();

			// @TODO: #3110 this is a total hack,
			// the tokens inside the cloned pst are still pointing to the original source,
			// this is just a good-ish, but total hack copy of a token source,
			// so we have a token source to construct the new PST with.
			auto token_source_hack
				= tokenizer::makeTokenSource(template_statement->getStablePosition()
			                                     .getActiveSourcePositionIllegalAccess()
			                                     .getSource()
			                                     ->getFile());

			auto baked_pst = pst::PST<pst::TemplateStmt>::fromClone(
				std::move(cloned),
				std::move(token_source_hack),
				hash_ctx
			);


			auto baked_root      = baked_pst.getRootElement().unlock(ctx);
			auto baked_statement = baked_root->getInnerStatement();

			baked_pst.setAdditionalRootData(pst::AdditionalRootData{
				.pst_parent = pst::AdditionalRootData::BakedTemplateParent{
					.template_bake_data = TemplateBakePSTLinkedData{
						.pst_parent_element
						= getPSTElementParent(ctx, template_statement).getAsLangElement(),
						.template_arguments_symbols = makeSharedBox<
							std::atomic<std::vector<SymID>*>,
							TemplateBakePSTLinkedData::TemplateArgumentsSymbolsDeleter>(nullptr
			            ) } } });

			auto args = bakeTemplateArgumentsSymbols(
				ctx, signature, ctx.query<QueryPrimaryCodeScopeFor>({ baked_root }), key
			);

			// @TODO: #3072 try to improve this.
			// We atomically set template_arguments_symbols here,
			// The reason we can't do it in the initial setAdditionalRootData call is because
			// we can't call QueryPrimaryCodeScopeFor(baked_root) without pst_parent_element set.
			// At the same time this scope is needed to generate the template argument symbols, so
			// we have to do it in two steps.
			CRef<TemplateBakePSTLinkedData> baked_root_data_pointer
				= std::any_cast<TemplateBakePSTLinkedData>(
					&baked_root->getAdditionalRootData()
						 .getAs<pst::AdditionalRootData::BakedTemplateParent>()
						 .template_bake_data
				);
			baked_root_data_pointer->template_arguments_symbols->store(
				new std::vector<SymID>(std::move(args)), std::memory_order_release
			);


			auto baked_sym_id = ctx.query<QuerySymbolOfSTMT>(baked_statement).valueOrThrow();

			return TemplateBakeStorage{ .baked_template_sym_id = baked_sym_id,
				                        .baked_template_pst    = std::move(baked_pst), };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA([](CRef<PResult> result) -> QResult {
			if (result->hasFailed()) return query::Failed();
			return result->valueOrPanic().baked_template_sym_id;
		})
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryBakeTemplateSymID);

}
