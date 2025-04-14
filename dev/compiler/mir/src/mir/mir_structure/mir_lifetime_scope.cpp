#include "mir_lifetime_scope.hpp"

namespace compiler::mir {

    namespace {
        constinit u64 next_id = 0;
        LifetimeScopeTree::LifetimeScope root_scope{
            .parent=nullptr, 
            .depth=0,
            .id=next_id++,
        };
    }

    LifetimeScopeTree::LifetimeScopeTree(): root(&root_scope) {}

    ScopeRef LifetimeScopeTree::newScope(CRef<LifetimeScope> parent) {
        auto key = scopes.emplaceBack(parent, parent->depth + 1, next_id++);
        return scopes.getCRef(key).value();
    }
}
