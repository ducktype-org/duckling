use std::collections::HashSet;
use std::hash::Hash;

pub trait QpExtend<A> {
    /// Extends `self` by `iter`.
    fn is_extended_by<T>(&mut self, iter: T) -> bool
    where
        T: IntoIterator<Item = A>,
        Self: Extend<A>;
}

impl<A> QpExtend<A> for HashSet<A>
where
    A: Eq + Hash,
{
    fn is_extended_by<T>(&mut self, iter: T) -> bool
    where
        T: IntoIterator<Item = A>,
    {
        let l1 = self.len();
        self.extend(iter);
        let l2 = self.len();
        l2 > l1
    }
}
