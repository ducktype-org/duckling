// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

// Temporary include file that allows to use all of the elements
// Should never be included in hpp files and is only for writing cpp files
// before optimizing includes

#warning "Including the pst_parser/elements/includes/all.hpp file is meant only for development"

#include "../hierarchy/actions/all_actions.hpp"                // IWYU pragma: export
#include "../hierarchy/class_elements/all_class_elements.hpp"  // IWYU pragma: export
#include "../hierarchy/expr_holders.hpp"                       // IWYU pragma: export
#include "../hierarchy/expressions/all_expr.hpp"               // IWYU pragma: export
#include "../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: export
#include "../hierarchy/meta.hpp"                               // IWYU pragma: export
#include "../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: export
#include "../hierarchy/statements/all_statements.hpp"          // IWYU pragma: export
