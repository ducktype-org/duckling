#include "mir_lifetime_scope.hpp"

namespace compiler::mir {

    namespace {
        /**
         * It starts at 1, so SUPER_ROOT_SCOPE can have id 0 created at compile-time.
         */
        constinit u64 next_id = 1;

        /**
         * Root scopes above all scopes trees.
         * It is a scope returned by getSuperRootScope() function.
         */
        constexpr LifetimeScopeTree::LifetimeScope SUPER_ROOT_SCOPE{
            .parent=nullptr, 
            .depth=0,
            .id=0,
        };
    }

    LifetimeScopeTree::LifetimeScopeTree():
        scopes(),
        root(newScope(&SUPER_ROOT_SCOPE)) { }

    ScopeRef LifetimeScopeTree::newScope(ScopeRef parent) {
        scopes.emplaceBack(parent, parent->depth + 1, next_id++);
        return scopes.last();
    }

    ScopeRef getSuperRootScope() {
        return &SUPER_ROOT_SCOPE;
    }
}
