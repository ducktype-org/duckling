#include "templates.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/template_stmt.hpp>
#include <frontend/pst_parser/lang_parser_context.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/symbols/symbol_id.hpp>
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
			auto type_ctv        = getTypeCTVFromPST(ctx, type_expression.unlock(ctx)->getExpr());
			if (type_ctv.hasFailed()) return query::Failed();

			// Template parameters are baked to constants so the type of the parameter is always
			// immutable.
			auto type = type_ctv.valueOrPanic().get<tsh::SymbolType<>>().value().withMutability(
				tsh::Mutability::Immutable
			);

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

		// @TODO: #3071 see if anything will have to be changed here
		pst::PST<pst::TemplateStmt> baked_template_pst;
	};

	void TemplateBakePSTLinkedData::PostponedDataDeleter::del(
		std::atomic<PostponedData*>* ptr
	) {
		// Here we delete both raw pointer allocated with new and the atomic wrapper around it.
		auto data_ptr = ptr->load(std::memory_order_acquire);
		if (data_ptr) delete data_ptr;
		delete ptr;
	}

	struct IMPLEMENT_QUERY(QueryBakeTemplateSymID, query::QResult<TemplateBakeStorage>) {
		static std::vector<SymID> bakeTemplateArgumentsSymbols(
			Context&                            ctx,
			const TemplateDeclarationSignature& signature,
			ScopeID                             scope,
			const QKey&                         q_key
		) {
			std::vector<SymID> symbols;

			u64 i = 0;
			for (const auto& param: signature.parameters) {
				auto ctv = q_key.template_arguments.at(i);
				i++;

				CORE_ASSERT(
					ctv.getTypeOfStoredValue(ctx).withMutability(tsh::Mutability::Immutable)
						== param.type,
					"Template argument type does not match template parameter type: ",
					ctv.getTypeOfStoredValue(ctx).toString(),
					" vs ",
					param.type.toString()
				);

				auto const_symbol
					= ctx.query<defgen::QueryGeneratedSymbol>(defgen::KeyFor_QueryGeneratedSymbol{
						.name                  = param.name,
						.generated_symbol_data = defgen::GeneratedConstant{ ctv, scope },
					});

				symbols.push_back(const_symbol);
			}

			return symbols;
		}

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// Most template heavy lifting happens here, and in usage of pst root data.

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
				"argument count does not match template parameter count"
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
				std::move(cloned), std::move(token_source_hack), hash_ctx
			);


			auto baked_root      = baked_pst.getRootElement().unlock(ctx);
			auto baked_statement = baked_root->getInnerStatement();

			baked_pst.setAdditionalRootData(pst::AdditionalRootData{
				.pst_parent = pst::AdditionalRootData::BakedTemplateParent{
					.template_bake_data = TemplateBakePSTLinkedData{
						.pst_parent_element
						= getPSTElementParent(ctx, template_statement).getAsLangElement(),
						.postponed_data = makeSharedBox<
							std::atomic<TemplateBakePSTLinkedData::PostponedData*>,
							TemplateBakePSTLinkedData::PostponedDataDeleter>(nullptr
			            ) } } });

			auto args = bakeTemplateArgumentsSymbols(
				ctx, signature, ctx.query<QueryPrimaryCodeScopeFor>({ baked_root }), key
			);
			auto baked_sym_id = ctx.query<QuerySymbolOfSTMT>(baked_statement).valueOrThrow();
			

			// @TODO: #3072 try to improve this.
			// We atomically set postponed_data here,
			// The reason we can't do it in the initial setAdditionalRootData call is because
			// we can't call QueryPrimaryCodeScopeFor(baked_root) or QuerySymbolOfSTMT(baked_statement) without pst_parent_element set (it is needed for relevant scopes be created).
			// At the same time this scope is needed to generate the template argument symbols, so
			// we have to do it in two steps.
			CRef<TemplateBakePSTLinkedData> baked_root_data_pointer
				= std::any_cast<TemplateBakePSTLinkedData>(
					&baked_root->getAdditionalRootData()
						 .getAs<pst::AdditionalRootData::BakedTemplateParent>()
						 .template_bake_data
				);
			baked_root_data_pointer->postponed_data->store(
				new TemplateBakePSTLinkedData::PostponedData{
					.baked_symbol = baked_sym_id,
					.template_arguments_symbols = std::move(args)
				},
				std::memory_order_release
			);

			return TemplateBakeStorage{
				.baked_template_sym_id = baked_sym_id,
				.baked_template_pst    = std::move(baked_pst),
			};
		}

		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA([](CRef<PResult> result) -> QResult {
			if (result->hasFailed()) return query::Failed();
			return result->valueOrPanic().baked_template_sym_id;
		})
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryBakeTemplateSymID);

}
