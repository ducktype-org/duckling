use std::{
    borrow::Borrow, convert::Infallible, ffi::OsStr, fmt::Display, ops::Deref, path::Path,
    str::FromStr,
};

use symbol_table::GlobalSymbol;

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Eq, PartialEq, PartialOrd, Ord, Hash, Serialize, Deserialize, Debug)]
pub struct StrId(GlobalSymbol);

impl StrId {
    pub fn new(string: impl AsRef<str>) -> Self {
        Self(GlobalSymbol::new(string))
    }

    pub fn as_str(&self) -> &'static str {
        self.0.as_str()
    }

    pub(crate) fn __from_static_helper(s: GlobalSymbol) -> Self {
        Self(s)
    }
}

macro_rules! static_str_id {
    ($x:literal) => {
        $crate::StrId::__from_static_helper(::symbol_table::static_symbol!($x))
    };
}

pub(crate) use static_str_id;

impl Default for StrId {
    fn default() -> Self {
        static_str_id!("")
    }
}

impl Display for StrId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl From<&str> for StrId {
    fn from(value: &str) -> Self {
        Self::new(value)
    }
}

impl From<&String> for StrId {
    fn from(value: &String) -> Self {
        Self::new(value.as_str())
    }
}

impl From<String> for StrId {
    fn from(value: String) -> Self {
        Self::new(value)
    }
}

impl FromStr for StrId {
    type Err = Infallible;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Self::new(s))
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_tests() {
        let a = StrId::new("a");
        let a_copy = StrId::new("a");
        assert_eq!(a, a_copy);

        assert_eq!(a, "a");
        assert_eq!(a, "a".to_owned());
        assert_eq!(a.to_string(), "a");
        assert_eq!(a.as_str(), "a");
    }
}
