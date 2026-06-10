//! Generic helper for adding a [`Version`] to various structures.

use serde::{Deserialize, Serialize};

use crate::quackpack::core::Version;

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// A struct, which also holds a version.
pub struct WithVersion<T> {
    value: T,
    version: Version,
}

impl<T> WithVersion<T> {
    /// Generate a new [`WithVersion`].
    pub fn new(value: T, version: Version) -> Self {
        Self { value, version }
    }

    /// Get a reference to the underlying value.
    pub fn value(&self) -> &T {
        &self.value
    }

    /// Get a mutable reference to the underlying value.
    pub fn value_mut(&mut self) -> &mut T {
        &mut self.value
    }

    /// Set the underlying value.
    pub fn set_value(&mut self, value: T) {
        self.value = value;
    }

    /// Get the version.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Set the version.
    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    /// Consume self, returing the underlying value.
    pub fn into_value(self) -> T {
        self.value
    }
}

impl<T> AsRef<T> for WithVersion<T> {
    fn as_ref(&self) -> &T {
        self.value()
    }
}

impl<T> AsMut<T> for WithVersion<T> {
    fn as_mut(&mut self) -> &mut T {
        self.value_mut()
    }
}
