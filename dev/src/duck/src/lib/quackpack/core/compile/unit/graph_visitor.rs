// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A [`UnitVisitor`], a visitor for [`Unit`]s of compilation.

use std::ops::ControlFlow;

use super::graph::UnitGraphNode;

/// A visitor of [`UnitGraph`](super::graph::UnitGraph).
///
/// The role of this trait is to gather information from the currently visited [`UnitGraphNode`].
pub trait GraphVisitor {
    /// Value reported by visitor if it stops visiting, for any reason.
    type Break;

    /// The role of this function is to gather information from the currently visited [`UnitGraphNode`].
    fn visit(&mut self, unit: &UnitGraphNode) -> ControlFlow<Self::Break>;
}

impl<V: GraphVisitor + ?Sized> GraphVisitor for &mut V {
    type Break = V::Break;
    fn visit(&mut self, unit: &UnitGraphNode) -> ControlFlow<Self::Break> {
        (*self).visit(unit)
    }
}

impl<V: GraphVisitor + ?Sized> GraphVisitor for Box<V> {
    type Break = V::Break;
    fn visit(&mut self, unit: &UnitGraphNode) -> ControlFlow<Self::Break> {
        (**self).visit(unit)
    }
}

/// A visitor of [`UnitGraph`](super::graph::UnitGraph).
///
/// The role of this trait is to gather information from the currently visited [`UnitGraphNode`].
///
/// Unlike [`GraphVisitor`], this visitor can fail.
pub trait TryGraphVisitor {
    /// Error reported by this visitor.
    type Err;

    /// Value reported by this visitor, if it stops visiting, for any reason.
    type Break;

    /// The role of this function is to gather information from the currently visited [`UnitGraphNode`].
    fn try_visit(&mut self, unit: &UnitGraphNode) -> Result<ControlFlow<Self::Break>, Self::Err>;
}

impl<V: TryGraphVisitor + ?Sized> TryGraphVisitor for &mut V {
    type Break = V::Break;
    type Err = V::Err;

    fn try_visit(&mut self, unit: &UnitGraphNode) -> Result<ControlFlow<Self::Break>, Self::Err> {
        (*self).try_visit(unit)
    }
}

impl<V: TryGraphVisitor + ?Sized> TryGraphVisitor for Box<V> {
    type Break = V::Break;
    type Err = V::Err;

    fn try_visit(&mut self, unit: &UnitGraphNode) -> Result<ControlFlow<Self::Break>, Self::Err> {
        (**self).try_visit(unit)
    }
}
