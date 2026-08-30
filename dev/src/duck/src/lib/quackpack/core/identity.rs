//! A simple package identity, a unique identifier in the dependencies' graph.
//! Difference between [`Identity`] and [`FullIdentity`] is that [`Identity`] doesn't have commits inside.
//!
//! Also, they have different [`Display`], [`Serialize`], and [`Deserialize`] impls.
//!
//! In particular, [`Identity`] and [`Origin`] implement [`FromStr`].
//!
//! For differences between [`FullIdentity`] and [`Identity`], see the `readme.md` under the `core/` directory.

use std::cmp::Ordering;
use std::fmt::Display;
use std::path::Path;
use std::str::FromStr;

use serde::{Deserialize, Serialize, de};

use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackError, QuackResult, QuackResultContext, StrId, qp_bail, qp_err};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
/// A simplified version of the [`FullIdentity`].
pub struct Identity {
    name: StrId,
    origin: Origin,
}

impl Identity {
    /// Create a new [`Identity`].
    pub fn new(name: StrId, origin: Origin) -> Self {
        Self { name, origin }
    }

    /// Get the name.
    pub fn name(self) -> StrId {
        self.name
    }

    /// Get the [`Origin`].
    pub fn origin(&self) -> Origin {
        self.origin
    }

    /// Compare `lhs` and `rhs` in a stable way!
    /// By “stable” we mean that the return type does not depend on:
    /// * used rust version,
    /// * current duck execution,
    /// * etc.
    ///
    /// Firstly we compare names lexicographically, then [`Origin`]s.
    pub fn stable_compare(lhs: Self, rhs: Self) -> Ordering {
        let name_cmp = lhs.name.cmp(&rhs.name);
        if name_cmp.is_ne() {
            return name_cmp;
        }
        Origin::stable_compare(lhs.origin, rhs.origin)
    }
}

impl PartialEq<FullIdentity> for Identity {
    fn eq(&self, other: &FullIdentity) -> bool {
        self.name() == other.name() && self.origin() == other.origin()
    }
}

impl PartialEq<Identity> for FullIdentity {
    fn eq(&self, other: &Identity) -> bool {
        other == self
    }
}

impl Display for Identity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.name, self.origin)
    }
}

impl FromStr for Identity {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let Some((name, origin)) = s.split_once(' ') else {
            qp_bail!("expected a simple identity in format `<name> <origin>`")
        };
        let origin = origin
            .parse()
            .with_context(|| format!("simple identity `{}` has invalid origin syntax", s))?;
        Ok(Self::new(name.into(), origin))
    }
}

impl Serialize for Identity {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for Identity {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a simple identity")
            .string(|s| s.parse().map_err(de::Error::custom))
            .deserialize(deserializer)
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// A simplified version of the [`Origin`].
pub struct Origin {
    url: InternedUrl,
    kind: Kind,
}

impl Origin {
    /// Create a new [`Origin`].
    pub(super) fn new(url: InternedUrl, kind: Kind) -> Self {
        // kind = local => url.is_local_file
        debug_assert!(
            url.is_local_file() || !kind.is_local(),
            "kind=`local` should have `file://`; has kind=`{kind}` and url=`{url}`"
        );
        Self { url, kind }
    }

    /// Create a new [`Origin`] for a registry.
    pub fn for_registry(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), Kind::Registry)
    }

    /// Create a new [`Origin`] for a git with a commit.
    pub fn for_git(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), Kind::Git)
    }

    /// Create a new [`Origin`] for a git with a commit.
    pub fn for_local(root: &Path) -> QuackResult<Self> {
        let url = root.to_url()?;
        Ok(Self::new(url.into(), Kind::Local))
    }

    /// Get an [`InternedUrl`] of this [`Origin`].
    pub fn url(self) -> InternedUrl {
        self.url
    }

    /// Get a [`Kind`] of this [`Origin`].
    pub fn kind(self) -> Kind {
        self.kind
    }

    /// Compare `lhs` and `rhs` in a stable way!
    /// By “stable” we mean that the return type does not depend on:
    /// * used rust version,
    /// * current duck execution,
    /// * etc.
    ///
    /// Firstly, we compare kinds, then URLs lexicographically.
    pub fn stable_compare(lhs: Self, rhs: Self) -> Ordering {
        let kind_ordering = Kind::stable_compare(lhs.kind, rhs.kind);
        if kind_ordering.is_ne() {
            return kind_ordering;
        }
        lhs.url.cmp(&rhs.url)
    }
}

impl PartialEq<FullOrigin> for Origin {
    fn eq(&self, other: &FullOrigin) -> bool {
        self.kind() == other.kind() && self.url() == other.url()
    }
}

impl PartialEq<Origin> for FullOrigin {
    fn eq(&self, other: &Origin) -> bool {
        other == self
    }
}

impl Display for Origin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}+{}", self.kind, self.url)
    }
}

impl FromStr for Origin {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let Some((kind, url)) = s.split_once('+') else {
            qp_bail!(
                "expected input to be in format `kind+url`, but found `{}`",
                s,
            )
        };

        let unexpected_kind_error = |kind: &str| qp_err!("inferred unsupported kind=`{kind}`");
        let kind = match kind {
            "registry" => Kind::Registry,
            "local" => Kind::Local,
            "git" => Kind::Git,
            kind => return Err(unexpected_kind_error(kind)),
        };
        let url = url.to_url()?;
        if kind.is_local() && !url.is_local_file() {
            qp_bail!(
                "inferred kind=`local`, but url scheme is `{}`",
                url.scheme()
            )
        }
        Ok(Self::new(url.into(), kind))
    }
}

impl Serialize for Origin {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for Origin {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a simple origin")
            .string(|s| s.parse().map_err(de::Error::custom))
            .deserialize(deserializer)
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// An [`Origin`]'s kind.
pub enum Kind {
    /// A registry-based identity.
    Registry,
    /// A git-based identity.
    Git,
    /// A local-based identity.
    Local,
}

impl Kind {
    /// Get a human-like display.
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Registry => "registry",
            Self::Git => "git",
            Self::Local => "local",
        }
    }

    /// Check, whether this [`Kind`] is a registry kind.
    pub fn is_registry(self) -> bool {
        matches!(self, Kind::Registry)
    }

    /// Check, whether this [`Kind`] is a git kind.
    pub fn is_git(self) -> bool {
        matches!(self, Kind::Git)
    }

    /// Check, whether this [`Kind`] is a local kind.
    pub fn is_local(self) -> bool {
        matches!(self, Kind::Local)
    }

    /// Compare `lhs` and `rhs` in a stable way!
    /// By “stable” we mean that the return type does not depend on:
    /// * used rust version,
    /// * current duck execution,
    /// * etc.
    ///
    /// Order is: [`Local`](Kind::Local) < [`Git`](Kind::Git) < [`Registry`](Kind::Registry).
    pub fn stable_compare(lhs: Self, rhs: Self) -> Ordering {
        match (lhs, rhs) {
            // Lhs == Rhs.
            (Self::Registry, Self::Registry)
            | (Self::Git, Self::Git)
            | (Self::Local, Self::Local) => Ordering::Equal,
            // Lhs > Rhs.
            (Self::Registry, Self::Git)
            | (Self::Registry, Self::Local)
            | (Self::Git, Self::Local) => Ordering::Greater,
            // Lhs < Rhs.
            (Self::Git, Self::Registry)
            | (Self::Local, Self::Registry)
            | (Self::Local, Self::Git) => Ordering::Less,
        }
    }
}

impl Display for Kind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl PartialEq<FullKind> for Kind {
    fn eq(&self, other: &FullKind) -> bool {
        matches!(
            (self, other),
            (Self::Registry, FullKind::Registry)
                | (Self::Git, FullKind::Git { .. })
                | (Self::Local, FullKind::Local)
        )
    }
}

impl PartialEq<Kind> for FullKind {
    fn eq(&self, other: &Kind) -> bool {
        other == self
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn origin_display() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            assert_eq!(origin.to_string(), "registry+https://localhost:9001/");
        }

        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_git(url);
            assert_eq!(origin.to_string(), "git+https://localhost:9001/");
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            #[cfg(windows)]
            assert_eq!(origin.to_string(), "local+file:///C:/");
            #[cfg(not(windows))]
            assert_eq!(origin.to_string(), "local+file:///tmp");
        }
    }

    #[test]
    fn origin_parse() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let formatted = origin.to_string();
            let parsed = formatted.parse::<Origin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_git(url);
            let formatted = origin.to_string();
            let parsed = formatted.parse::<Origin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let formatted = origin.to_string();
            let parsed = formatted.parse::<Origin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let unknown_kind = "foo+https://localhost:9001/";
            let err = unknown_kind.parse::<Origin>().unwrap_err();
            assert_eq!(err.to_string(), "inferred unsupported kind=`foo`")
        }

        {
            let local_wrong_scheme = "local+https://localhost:9001/";
            let err = local_wrong_scheme.parse::<Origin>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`local`, but url scheme is `https`"
            )
        }

        {
            let wrong_format = "https://localhost:9001/";
            let err = wrong_format.parse::<Origin>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected input to be in format `kind+url`, but found `https://localhost:9001/`"
            )
        }
    }

    #[test]
    fn identity_display() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo registry+https://localhost:9001/");
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_git(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo git+https://localhost:9001/");
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            #[cfg(windows)]
            assert_eq!(formatted, "foo local+file:///C:/");
            #[cfg(not(windows))]
            assert_eq!(formatted, "foo local+file:///tmp");
        }
    }

    #[test]
    fn identity_stable_sort() {
        let url = "https://localhost:9001".to_url().unwrap();
        let origin = Origin::for_registry(url);
        let foo = Identity::new("foo".into(), origin);
        let url = "https://localhost:9001".to_url().unwrap();
        let origin = Origin::for_git(url);
        let foo_git = Identity::new("foo".into(), origin);
        #[cfg(windows)]
        let root = PathBuf::from("C:\\");
        #[cfg(not(windows))]
        let root = PathBuf::from("/tmp");
        let origin = Origin::for_local(&root).unwrap();
        let foo_local = Identity::new("foo".into(), origin);

        let url = "https://localhost:9001".to_url().unwrap();
        let origin = Origin::for_registry(url);
        let bar = Identity::new("bar".into(), origin);
        let url = "https://localhost:9001".to_url().unwrap();
        let origin = Origin::for_git(url);
        let bar_git = Identity::new("bar".into(), origin);
        #[cfg(windows)]
        let root = PathBuf::from("C:\\");
        #[cfg(not(windows))]
        let root = PathBuf::from("/tmp");
        let origin = Origin::for_local(&root).unwrap();
        let bar_local = Identity::new("bar".into(), origin);

        assert_eq!(Identity::stable_compare(foo, foo), Ordering::Equal);
        assert_eq!(Identity::stable_compare(foo, foo_git), Ordering::Greater);
        assert_eq!(Identity::stable_compare(foo, foo_local), Ordering::Greater);
        assert_eq!(Identity::stable_compare(foo, bar), Ordering::Greater);
        assert_eq!(Identity::stable_compare(foo, bar_git), Ordering::Greater);
        assert_eq!(Identity::stable_compare(foo, bar_local), Ordering::Greater);

        assert_eq!(Identity::stable_compare(foo_git, foo), Ordering::Less);
        assert_eq!(Identity::stable_compare(foo_git, foo_git), Ordering::Equal);
        assert_eq!(
            Identity::stable_compare(foo_git, foo_local),
            Ordering::Greater
        );
        assert_eq!(Identity::stable_compare(foo_git, bar), Ordering::Greater);
        assert_eq!(
            Identity::stable_compare(foo_git, bar_git),
            Ordering::Greater
        );
        assert_eq!(
            Identity::stable_compare(foo_git, bar_local),
            Ordering::Greater
        );

        assert_eq!(Identity::stable_compare(foo_local, foo), Ordering::Less);
        assert_eq!(Identity::stable_compare(foo_local, foo_git), Ordering::Less);
        assert_eq!(
            Identity::stable_compare(foo_local, foo_local),
            Ordering::Equal
        );
        assert_eq!(Identity::stable_compare(foo_local, bar), Ordering::Greater);
        assert_eq!(
            Identity::stable_compare(foo_local, bar_git),
            Ordering::Greater
        );
        assert_eq!(
            Identity::stable_compare(foo_local, bar_local),
            Ordering::Greater
        );

        assert_eq!(Identity::stable_compare(bar, foo), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar, foo_git), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar, foo_local), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar, bar), Ordering::Equal);
        assert_eq!(Identity::stable_compare(bar, bar_git), Ordering::Greater);
        assert_eq!(Identity::stable_compare(bar, bar_local), Ordering::Greater);

        assert_eq!(Identity::stable_compare(bar_git, foo), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_git, foo_git), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_git, foo_local), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_git, bar), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_git, bar_git), Ordering::Equal);
        assert_eq!(
            Identity::stable_compare(bar_git, bar_local),
            Ordering::Greater
        );

        assert_eq!(Identity::stable_compare(bar_local, foo), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_local, foo_git), Ordering::Less);
        assert_eq!(
            Identity::stable_compare(bar_local, foo_local),
            Ordering::Less
        );
        assert_eq!(Identity::stable_compare(bar_local, bar), Ordering::Less);
        assert_eq!(Identity::stable_compare(bar_local, bar_git), Ordering::Less);
        assert_eq!(
            Identity::stable_compare(bar_local, bar_local),
            Ordering::Equal
        );
    }

    #[test]
    fn identity_parse() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<Identity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_git(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<Identity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let identity = Identity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<Identity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let invalid_format = "foo";
            let err = invalid_format.parse::<Identity>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected a simple identity in format `<name> <origin>`"
            )
        }

        {
            let invalid_origin_format = "foo local+";
            let err = invalid_origin_format.parse::<Identity>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "simple identity `foo local+` has invalid origin syntax
`` is not a valid url
relative URL without a base"
            )
        }
    }
}
