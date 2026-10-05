// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A [`UnitVisitor`], a visitor for [`Unit`]s of compilation.

use std::ops::ControlFlow;

use super::Unit;

/// A visitor of [`Unit`]s.
///
/// The role of this trait is to gather information from the currently visited [`Unit`].
pub trait UnitVisitor {
    /// Value reported by visitor if it stops visiting, for any reason.
    type Break;

    /// The role of this function is to gather information from the currently visited [`Unit`].
    fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break>;
}

impl<V: UnitVisitor + ?Sized> UnitVisitor for &mut V {
    type Break = V::Break;
    fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
        (*self).visit(unit)
    }
}

impl<V: UnitVisitor + ?Sized> UnitVisitor for Box<V> {
    type Break = V::Break;
    fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
        (**self).visit(unit)
    }
}

/// A visitor of [`Unit`]s.
///
/// The role of this trait is to gather information from the currently visited [`Unit`].
///
/// Unlike [`UnitVisitor`], this visitor can fail.
pub trait TryUnitVisitor {
    /// Error reported by this visitor.
    type Err;

    /// Value reported by this visitor, if it stops visiting, for any reason.
    type Break;

    /// The role of this function is to gather information from the currently visited [`Unit`].
    fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err>;
}

impl<V: TryUnitVisitor + ?Sized> TryUnitVisitor for &mut V {
    type Break = V::Break;
    type Err = V::Err;

    fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
        (*self).try_visit(unit)
    }
}

impl<V: TryUnitVisitor + ?Sized> TryUnitVisitor for Box<V> {
    type Break = V::Break;
    type Err = V::Err;

    fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
        (**self).try_visit(unit)
    }
}
