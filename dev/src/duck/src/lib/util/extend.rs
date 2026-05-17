use std::collections::HashSet;
use std::hash::Hash;

<<<<<<< HEAD
pub trait QpExtend<A>: Extend<A> {
    /// Extends `self` by `iter` and return the number of new elements.
    fn extend_and_get_diff_size<T>(&mut self, iter: T) -> usize
    where
        T: IntoIterator<Item = A>;
=======
pub trait QpExtend<A> {
    /// Extends `self` by `iter`.
    fn is_extended_by<T>(&mut self, iter: T) -> bool
    where
        T: IntoIterator<Item = A>,
        Self: Extend<A>;
>>>>>>> 9fa86ce66 (Change the implementation of populate_features + fix tests)
}

impl<A> QpExtend<A> for HashSet<A>
where
    A: Eq + Hash,
{
<<<<<<< HEAD
    fn extend_and_get_diff_size<T>(&mut self, iter: T) -> usize
=======
    fn is_extended_by<T>(&mut self, iter: T) -> bool
>>>>>>> 9fa86ce66 (Change the implementation of populate_features + fix tests)
    where
        T: IntoIterator<Item = A>,
    {
        let l1 = self.len();
<<<<<<< HEAD
        Extend::extend(self, iter);
        let l2 = self.len();
        l2 - l1
=======
        self.extend(iter);
        let l2 = self.len();
        l2 > l1
>>>>>>> 9fa86ce66 (Change the implementation of populate_features + fix tests)
    }
}
