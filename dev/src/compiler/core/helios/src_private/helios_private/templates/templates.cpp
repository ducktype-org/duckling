#include "templates.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/template_decl.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp> // PR relax it?
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp> //PR relax it?
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp> // PR relax it?
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/lang_parser_context.hpp>

#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
// #include <helios_private/hout_creation/definition_generation/
#include <helios/tsh/deductions.hpp>

#include <query_framework/standard_query/query_impl.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <hashing/add_to_hash.hpp>

namespace compiler::helios::templates {

    base::Bit256 TemplateBakeKey::queryUnstablePerfectHash() const {
        hashing::SHA256 hasher;
        hashing::addToHash(hasher, template_sym_id.queryUnstablePerfectHash());
        hashing::addToHash(hasher, template_arguments.size());

        for (const auto& arg: template_arguments) {
            hashing::addToHash(hasher, arg.queryUnstablePerfectHash());
        }

        return hasher.finalize();
    }


    struct TemplateBakeStorage final {
        SymID baked_template_sym_id;

        // TODO: add custom pst element todo issue (link root pst data one) 
        pst::PST<pst::TemplateDecl> baked_template_pst;
    };


    struct IMPLEMENT_QUERY(QueryBakeTemplateSymID, query::QResult<TemplateBakeStorage>) {

        static std::vector<SymID> bakeTemplateArgumentsSymbols(Context& ctx, pst::Access<pst::ParamList> template_params, ScopeID scope) {
    

            std::vector<SymID> symbols;
            
            for (const auto& param: *template_params) {
                auto param_unlocked = param.unlock(ctx);

                auto type_expression = param_unlocked->getType();
                auto value_expression = param_unlocked->getValue()->unlock(ctx)->getExpr();
                auto name = param_unlocked->getName().unlock(ctx)->unwrap();

                auto type_ctv = getTypeCTVFromPST(ctx, type_expression.unlock(ctx)->getExpr()).valueOrThrow();

                auto type = tsh::deductions::declarationTypeFromProvidedType(
                        type_ctv.get<tsh::SymbolType<>>().value(),
                        tsh::Mutability::Immutable
                    );

                const auto hout_qresult = getHoutOfExprWithExpectedType(
    				ctx, value_expression, 
                   type
	    		);

                auto value_ctv
	    			= ctx.query<QueryEvaluateHOUTExpression>({ hout_qresult.valueOrThrow().ref() }).valueOrThrow();

                auto const_symbol = ctx.query<defgen::QueryGeneratedSymbol>(defgen::KeyFor_QueryGeneratedSymbol{
                    .name = name,
                    .generated_symbol_data = defgen::GeneratedSymbolData{defgen::GeneratedSymbolData::TemplateBakeConstant{
                        type, std::move(value_ctv), scope
                    }},
                });

                symbols.push_back(const_symbol);
            }

            return symbols;
        }


        static auto provide(Context& ctx, QKey key) -> PResult {
            // most heavy lifting will happen here, and in usage of pst root data

            // outline
            // 1. Assert template symbol ID is valid and is indeed a template
            // 2. Assert template arguments are valid for the template (count, types, etc.)
            // 3. prepare TemplateInstantiationData
            // 4. generate PST-copy with clone machinery and custom data for template arguments
            //    this design is not perfect, but it will fly for now, we will have to change it once we remove root_data in favor
            //    of proper PST nodes with data, but it will require more work, so let's do it later
            // 5. generate symbol ID for the baked template, and return it, linking the new pst-root as its pst-data
            //
            // NOTE: we will likely not generate template parameter constants here,
            // but rather in query scopes. Although, we could generate them here and link them to the baked symbol ID / TemplateInstantiationData
            //
            // NOTE: it might be good to put TemplateInstantiationData in symbol data,
            // and put just SymID in PST root data

            CORE_ASSERT(kind(key.template_sym_id) == SymbolKind::Template, "SymID is not a Template");

            auto pst_statement = stmt(ctx, key.template_sym_id).value();
            auto template_statement = pst_statement.dynamicCast<pst::TemplateDecl>().value();

            auto template_params = template_statement->getParams().unlock(ctx);

            // This should be an error:!!
            CORE_ASSERT(
                template_params->size() == key.template_arguments.size(),
                "Template arguments count does not match template parameters count"
            );

            // pass this to clone:?
            // auto dummy_token_source = template_statement->getStablePosition().getActiveSourcePosition(ctx).getSource();

            // TODO: get hash from context???
            hashing::ComponentHash hash_ctx = hashing::ComponentHash(base::StrID(key.queryUnstablePerfectHash().toStringHex()));

            
            std::cerr << "Running clone now!  \n";
            
            auto cloned = template_statement->clone().dynamicCast<pst::TemplateDecl>().toOptBox().value();
            
            std::cerr << "Baking pst now!  \n";

            auto token_source_hack = tokenizer::makeTokenSource(template_statement->getStablePosition().getActiveSourcePositionIllegalAccess().getSource()->getFile());

            auto baked_pst = pst::PST<pst::TemplateDecl>::fromClone(
                std::move(cloned),
                std::move(token_source_hack),
                pst::LangParserContext::programBaseContext(), // ???,
                hash_ctx // ???

                // context...?
                // hash...? from key hash + from template hash 
            );
            

            std::cerr << "Baked template pst!  \n";

            auto baked_root = baked_pst.getRootElement().unlock(ctx);
            auto baked_statement = baked_root->getInnerStatement();
            
            std::cerr << "Baked statement!  \n";


            auto baked_sym_id = ctx.query<QuerySymbolOfSTMT>(baked_statement).valueOrThrow();

            std::cerr << "Baked template sym_id: " << name(baked_sym_id).strView() << "\n";

            baked_pst.setAdditionalRootData(pst::AdditionalRootData{
                .pst_parent = pst::AdditionalRootData::TemplateParent{
                    .template_bake_data = TemplateBakePSTLinkedData{
                        // .instantiated_sym_id = SymID{}, // TODO: generate new sym_id for baked template
                        .pst_parent_element = template_statement->getParent().value(), // TODO: change to pst layer call.. templtaes in macros :o
                        .template_arguments_symbols = bakeTemplateArgumentsSymbols(ctx, template_params, 
                        
                            ctx.query<QueryPrimaryCodeScopeFor>({ baked_root })
                        )
                    }
                }
            });

            return TemplateBakeStorage{
                .baked_template_sym_id = baked_sym_id,
                .baked_template_pst = std::move(baked_pst)
            };
        }

        QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(
            [](CRef<PResult> result) -> QResult {
                if (result->hasFailed()) return query::Failed();
                return result->valueOrPanic().baked_template_sym_id;
            }
        )

    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryBakeTemplateSymID);
    
}