//! A [`StrId`], an interned version of a string (also known as a fly string).
//!
//! It's trivially copyable.
use std::{
    borrow::{Borrow, Cow},
    collections::HashSet,
    convert::Infallible,
    ffi::{OsStr, OsString},
    fmt::{Debug, Display},
    hash::Hash,
    ops::Deref,
    path::{Path, PathBuf},
    str::FromStr,
    sync::{Mutex, OnceLock},
};

use serde::{Deserialize, Serialize};

use crate::quackpack::util::PANIC_MESSAGE;

static STRID_CACHE: OnceLock<Mutex<HashSet<&'static str>>> = OnceLock::new();

#[derive(Clone, Copy)]
/// An interned/cached string.
pub struct StrId {
    inner: &'static str,
}

impl StrId {
    /// Get the inner [`str`] as a static.
    pub fn as_str(&self) -> &'static str {
        self.inner
    }

    /// Construct a new [`StrId`].
    pub fn new<'a>(s: impl Into<Cow<'a, str>>) -> Self {
        Self::from(s.into())
    }
}

impl Default for StrId {
    fn default() -> Self {
        Self::from("")
    }
}

impl From<&str> for StrId {
    fn from(value: &str) -> Self {
        Self::from(Cow::Borrowed(value))
    }
}

impl From<&String> for StrId {
    fn from(value: &String) -> Self {
        Self::from(value.as_str())
    }
}

impl From<String> for StrId {
    fn from(value: String) -> Self {
        Self::from(Cow::Owned(value))
    }
}

impl From<&Path> for StrId {
    fn from(value: &Path) -> Self {
        Self::from(value.to_string_lossy())
    }
}

impl From<PathBuf> for StrId {
    fn from(value: PathBuf) -> Self {
        Self::from(value.to_string_lossy())
    }
}

impl From<&OsStr> for StrId {
    fn from(value: &OsStr) -> Self {
        Self::from(value.to_string_lossy())
    }
}

impl From<OsString> for StrId {
    fn from(value: OsString) -> Self {
        Self::from(value.to_string_lossy())
    }
}

impl From<&OsString> for StrId {
    fn from(value: &OsString) -> Self {
        Self::from(value.to_string_lossy())
    }
}

impl From<Cow<'_, str>> for StrId {
    fn from(value: Cow<'_, str>) -> Self {
        let mut cache = STRID_CACHE
            .get_or_init(Default::default)
            .lock()
            // NOTE: `.unwrap()` should never panic: from docs:
            // Errors
            //
            // If another user of this mutex panicked while holding the mutex,
            // then this call will return an error once the mutex is acquired.
            // The acquired mutex guard will be contained in the returned error.
            //
            // Panics
            //
            // This function might panic when called if the lock is already held by the current thread.
            .expect(PANIC_MESSAGE);
        let reference = cache.get(value.as_ref()).copied().unwrap_or_else(|| {
            let static_ref = value.into_owned().leak();
            cache.insert(static_ref);
            static_ref
        });
        StrId { inner: reference }
    }
}

impl FromStr for StrId {
    type Err = Infallible;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Self::from(s))
    }
}

impl PartialEq<str> for StrId {
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

impl PartialEq<&str> for StrId {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<String> for StrId {
    fn eq(&self, other: &String) -> bool {
        self.as_str() == other.as_str()
    }
}

impl PartialEq<StrId> for StrId {
    fn eq(&self, other: &StrId) -> bool {
        self.inner == other.inner
    }
}

impl Eq for StrId {}

impl PartialOrd for StrId {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for StrId {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.inner.cmp(other.inner)
    }
}

impl Deref for StrId {
    type Target = str;

    fn deref(&self) -> &'static Self::Target {
        self.as_str()
    }
}

impl AsRef<str> for StrId {
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl AsRef<OsStr> for StrId {
    fn as_ref(&self) -> &OsStr {
        self.as_str().as_ref()
    }
}

impl AsRef<Path> for StrId {
    fn as_ref(&self) -> &Path {
        self.as_str().as_ref()
    }
}

impl Borrow<str> for StrId {
    fn borrow(&self) -> &str {
        self.as_str()
    }
}

impl Hash for StrId {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.inner.hash(state);
    }
}

impl Debug for StrId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <str as Debug>::fmt(self.inner, f)
    }
}

impl Display for StrId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <str as Display>::fmt(self.inner, f)
    }
}

impl From<StrId> for String {
    fn from(value: StrId) -> Self {
        value.as_str().into()
    }
}

impl From<StrId> for &'static str {
    fn from(value: StrId) -> Self {
        value.as_str()
    }
}

impl Serialize for StrId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.inner.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for StrId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let str = <&'de str>::deserialize(deserializer)?;
        Ok(Self::from(str))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_tests() {
        let a = StrId::from("a");
        let a_copy = StrId::from("a");
        assert_eq!(a, a_copy);

        assert_eq!(a, "a");
        assert_eq!(a, "a".to_owned());
        assert_eq!(a.to_string(), "a");
        assert_eq!(a.as_str(), "a");
    }
}
