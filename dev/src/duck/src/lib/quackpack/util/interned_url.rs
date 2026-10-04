//! An interned version of the [`Url`].

use std::collections::HashSet;
use std::fmt::{Debug, Display};
use std::hash::{Hash, Hasher};
use std::ops::{Deref, Index, Range, RangeFrom, RangeFull, RangeTo};
use std::str::FromStr;
use std::sync::{Mutex, OnceLock};

use serde::{Deserialize, Serialize};
use url::{Position, Url};

use crate::util::extract::Extract;

static URL_CACHE: OnceLock<Mutex<HashSet<&'static Url>>> = OnceLock::new();

#[derive(Clone, Copy, PartialOrd, Ord)] // Note: We can't forward {Partial}Ord to the pointers :(.
/// An interned version of the [`Url`].
pub struct InternedUrl {
    inner: &'static Url,
}

impl PartialEq for InternedUrl {
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self.inner, other.inner)
    }
}

impl PartialEq<Url> for InternedUrl {
    fn eq(&self, other: &Url) -> bool {
        self.as_url() == other
    }
}

impl PartialEq<&Url> for InternedUrl {
    fn eq(&self, other: &&Url) -> bool {
        self.as_url() == *other
    }
}

impl PartialEq<InternedUrl> for Url {
    fn eq(&self, other: &InternedUrl) -> bool {
        other == self
    }
}

impl PartialEq<InternedUrl> for &Url {
    fn eq(&self, other: &InternedUrl) -> bool {
        other == self
    }
}

impl Eq for InternedUrl {}

impl Hash for InternedUrl {
    fn hash<H: Hasher>(&self, state: &mut H) {
        std::ptr::hash(self.inner, state)
    }
}

impl InternedUrl {
    /// Create a new [`InternedUrl`].
    pub fn new(url: Url) -> Self {
        let mut cache = URL_CACHE.get_or_init(Default::default).lock().extract();
        let reference = cache.get(&url).copied().unwrap_or_else(|| {
            let static_ref = Box::leak(Box::new(url));
            cache.insert(static_ref);
            static_ref
        });
        Self { inner: reference }
    }

    /// Get the reference to the underlying [`Url`].
    pub fn as_url(self) -> &'static Url {
        self.inner
    }
}

impl From<Url> for InternedUrl {
    fn from(value: Url) -> Self {
        Self::new(value)
    }
}

impl Deref for InternedUrl {
    type Target = Url;

    fn deref(&self) -> &'static Self::Target {
        self.inner
    }
}

impl AsRef<Url> for InternedUrl {
    fn as_ref(&self) -> &'static Url {
        self.inner
    }
}

impl AsRef<str> for InternedUrl {
    fn as_ref(&self) -> &str {
        self.inner.as_ref()
    }
}

impl FromStr for InternedUrl {
    type Err = <Url as FromStr>::Err;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let url = Url::from_str(s)?;
        Ok(Self::new(url))
    }
}

impl Serialize for InternedUrl {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.inner.serialize(serializer)
    }
}

impl<'a> TryFrom<&'a str> for InternedUrl {
    type Error = <Url as TryFrom<&'a str>>::Error;

    fn try_from(value: &'a str) -> Result<Self, Self::Error> {
        let url = Url::try_from(value)?;
        Ok(Self::new(url))
    }
}

impl<'de> Deserialize<'de> for InternedUrl {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let url = Url::deserialize(deserializer)?;
        Ok(Self::new(url))
    }
}

impl Display for InternedUrl {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Display::fmt(self.inner, f)
    }
}

impl Debug for InternedUrl {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // We intentionally use Display for str here: Url's Debug gives internal data, whereas mostly
        // we want to see the human-friendly Url.
        Display::fmt(self.inner.as_str(), f)
    }
}

impl Index<Range<Position>> for InternedUrl {
    type Output = <Url as Index<Range<Position>>>::Output;

    fn index(&self, index: Range<Position>) -> &Self::Output {
        self.inner.index(index)
    }
}

impl Index<RangeFull> for InternedUrl {
    type Output = <Url as Index<RangeFull>>::Output;

    fn index(&self, index: RangeFull) -> &Self::Output {
        self.inner.index(index)
    }
}

impl Index<RangeTo<Position>> for InternedUrl {
    type Output = <Url as Index<RangeTo<Position>>>::Output;

    fn index(&self, index: RangeTo<Position>) -> &Self::Output {
        self.inner.index(index)
    }
}

impl Index<RangeFrom<Position>> for InternedUrl {
    type Output = <Url as Index<RangeFrom<Position>>>::Output;

    fn index(&self, index: RangeFrom<Position>) -> &Self::Output {
        self.inner.index(index)
    }
}
