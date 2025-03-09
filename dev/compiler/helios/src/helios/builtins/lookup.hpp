#pragma once

#include "../lookup_result.hpp"

namespace compiler::helios::builtin {

    /**
     * Lookup a builtin symbol by name.
     */
    LookupResult lookup(base::StrID name);

}