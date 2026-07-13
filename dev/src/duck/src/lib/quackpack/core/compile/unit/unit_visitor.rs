//! A [`UnitVisitor`], a visitor for [`Unit`]s of compilation.

use super::Unit;

/// A visitor of [`Unit`]s.
///
/// The role of this trait is to gather information from the currently visited [`Unit`].
pub trait UnitVisitor {
    /// The role of this function is to gather information from the currently visited [`Unit`].
    fn visit(&mut self, unit: &Unit);
}

impl<V: UnitVisitor + ?Sized> UnitVisitor for &mut V {
    fn visit(&mut self, unit: &Unit) {
        (*self).visit(unit);
    }
}

impl<V: UnitVisitor + ?Sized> UnitVisitor for Box<V> {
    fn visit(&mut self, unit: &Unit) {
        (**self).visit(unit);
    }
}
