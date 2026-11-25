// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::debug_assert;

use russcip::Variable;
use russcip::builder::cons::ConsBuilder;
use russcip::prelude::cons;

/// An extension of [`ConsBuilder`] with functions for representing binary variable implications as constraints.
pub trait ConsBuilderExt<'a> {
    /// An implication of form when_all_1 ∧ ... ∧ when_all_n => then_any_1 ∨ ... ∨ then_any_m.
    fn all_implies_any(when_all: Vec<&'a Variable>, then_any: Vec<&'a Variable>) -> Self;
    /// An implication of form when_all_1 ∧ ... ∧ when_all_n => then_all_1 ∧ ... ∧ then_all_m.
    fn all_implies_all(when_all: Vec<&'a Variable>, then_all: Vec<&'a Variable>) -> Self;
    /// Allow at most one variable to be evaluated true.
    fn at_most_one(vars: Vec<&'a Variable>) -> Self;
    /// Allow none of the variables to be evaluated true.
    fn none(vars: Vec<&'a Variable>) -> Self;
}

impl<'a> ConsBuilderExt<'a> for ConsBuilder<'a> {
    fn all_implies_any(
        when_all: Vec<&'a Variable>,
        then_any: Vec<&'a Variable>,
    ) -> ConsBuilder<'a> {
        debug_assert!(!when_all.is_empty());
        let when_all_len = when_all.len();
        let then_any_len = then_any.len();
        cons()
            .coefs(when_all, vec![1.0; when_all_len])
            .coefs(then_any, vec![-1.0; then_any_len])
            .le((when_all_len - 1) as f64)
    }

    fn all_implies_all(
        when_all: Vec<&'a Variable>,
        then_all: Vec<&'a Variable>,
    ) -> ConsBuilder<'a> {
        debug_assert!(!when_all.is_empty());
        let when_all_len = when_all.len();
        let then_all_len = then_all.len();
        cons()
            .coefs(when_all, vec![then_all_len as f64; when_all_len])
            .coefs(then_all, vec![-1.0; then_all_len])
            .le(((when_all_len - 1) * then_all_len) as f64)
    }

    fn at_most_one(vars: Vec<&'a Variable>) -> ConsBuilder<'a> {
        let vars_len = vars.len();
        cons().coefs(vars, vec![1.0; vars_len]).le(1.0)
    }

    fn none(vars: Vec<&'a Variable>) -> ConsBuilder<'a> {
        let vars_len = vars.len();
        cons().coefs(vars, vec![1.0; vars_len]).le(0.0)
    }
}
