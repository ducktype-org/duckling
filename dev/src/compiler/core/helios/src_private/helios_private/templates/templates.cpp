#include "templates.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/template_decl.hpp>

#include <helios/symbols/symbol_id_utils.hpp>

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


    struct IMPLEMENT_QUERY(QueryBakeTemplateSymID, query::QResult<SymID>) {
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

            CORE_PANIC("QueryBakeTemplateSymID is not implemented yet");
        }

        QUERY_AUTO_CACHE_COPY

    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryBakeTemplateSymID);
    
}