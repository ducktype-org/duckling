//! A value which can only be set once.
//!
//! Precisely, this looks like a [`bool`], but with a major difference: initially it is always `false`,
//! and can only be set to `true`.

use std::fmt;

#[derive(Clone, Copy, Hash, Eq, PartialEq, Ord, PartialOrd)]
/// A boolean value which can only be set to true.
pub struct SetOnce {
    was_set: bool,
}

impl SetOnce {
    /// Create new, unset [`SetOnce`].
    pub fn new() -> Self {
        Self { was_set: false }
    }

    /// Set this value.
    ///
    /// After this operation, all future calls to [`was_set`](Self::was_set) will return true.
    ///
    /// Note, that this operation cannot be undone.
    pub fn set(&mut self) {
        self.was_set = true;
    }

    /// Check, if this value was set (equivalently, was [`set`](Self::set) called on this value).
    pub fn was_set(&self) -> bool {
        self.was_set
    }
}

impl Default for SetOnce {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Display for SetOnce {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = if self.was_set() {
            "was set"
        } else {
            "was not set"
        };
        write!(f, "{}", text)
    }
}

impl fmt::Debug for SetOnce {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        <Self as fmt::Display>::fmt(self, f)
    }
}
