// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

/**
 * Macro that emulates short-circuit (lazy) implication.
 * Usage:
 * LAZY_IMPLIES(cond1, cond2) is equivalent to ((!(cond1)) || (cond2))
 */
#define LAZY_IMPLIES(cond1, cond2) ((!(cond1)) || (cond2))
