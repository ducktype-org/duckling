#include "templates.hpp"


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
            

            CORE_PANIC("QueryBakeTemplateSymID is not implemented yet");
        }

        QUERY_AUTO_CACHE_COPY

    };

    QUERY_IMPLEMENTATION_BOILERPLATE(QueryBakeTemplateSymID);
    
}