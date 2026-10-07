// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::HashSet;
use std::hash::Hash;

pub trait QpExtend<A>: Extend<A> {
    /// Extends `self` by `iter` and return the number of new elements.
    fn extend_and_get_diff_size<T>(&mut self, iter: T) -> usize
    where
        T: IntoIterator<Item = A>;
}

impl<A> QpExtend<A> for HashSet<A>
where
    A: Eq + Hash,
{
    fn extend_and_get_diff_size<T>(&mut self, iter: T) -> usize
    where
        T: IntoIterator<Item = A>,
    {
        let l1 = self.len();
        Extend::extend(self, iter);
        let l2 = self.len();
        l2 - l1
    }
}
