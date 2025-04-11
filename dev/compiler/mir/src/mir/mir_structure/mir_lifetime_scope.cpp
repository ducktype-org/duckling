#include "mir_lifetime_scope.hpp"

namespace compiler::mir {

    namespace {
        LifetimeScopeTree::LifetimeScope root_scope(nullptr, 0);
    }

    LifetimeScopeTree::LifetimeScopeTree(): root(&root_scope) {}

    ScopeRef LifetimeScopeTree::newScope(CRef<LifetimeScope> parent) {
        auto key = scopes.emplaceBack(parent, parent->depth + 1);
        return scopes.getCRef(key).value();
    }
}
