#pragma once

/**
 * Macro that emulates short-circuit (lazy) implication.
 * Usage:
 * LAZY_IMPLIES(cond1, cond2) is equivalent to ((!(cond1)) || (cond2))
 */
#define LAZY_IMPLIES(cond1, cond2) ((!(cond1)) || (cond2))
