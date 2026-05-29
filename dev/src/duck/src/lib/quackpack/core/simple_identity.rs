//! A simple package identity, a unique identifier in the dependencies' graph.
//! Difference between [`SimpleIdentity`] and [`Identity`] is that [`SimpleIdentity`] doesn't have commits inside.
//!
//! Also, they have different [`Display`], [`Serialize`], and [`Deserialize`] impls.
//!
//! In particular, [`SimpleIdentity`] and [`SimpleOrigin`] implement [`FromStr`].

use std::fmt::Display;
use std::path::Path;
use std::str::FromStr;

use serde::{Deserialize, Serialize, de};
use url::Url;

use crate::quackpack::core::identity::{Identity, Kind, Origin};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::{QuackError, QuackResult, QuackResultContext, StrId, qp_bail, qp_err};

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// An [`Origin`]'s kind.
pub enum SimpleKind {
    /// A registry-based identity.
    Registry,
    /// A git-based identity.
    Git,
    /// A local-based identity.
    Local,
}

impl SimpleKind {
    /// Get a human-like display.
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Registry => "registry",
            Self::Git => "git",
            Self::Local => "local",
        }
    }

    /// Check, whether this [`SimpleKind`] is a registry kind.
    pub fn is_registry(&self) -> bool {
        matches!(self, SimpleKind::Registry)
    }

    /// Check, whether this [`SimpleKind`] is a git kind.
    pub fn is_git(&self) -> bool {
        matches!(self, SimpleKind::Git)
    }

    /// Check, whether this [`SimpleKind`] is a local kind.
    pub fn is_local(&self) -> bool {
        matches!(self, SimpleKind::Local)
    }
}

impl Display for SimpleKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl PartialEq<Kind> for SimpleKind {
    fn eq(&self, other: &Kind) -> bool {
        matches!(
            (self, other),
            (Self::Registry, Kind::Registry)
                | (Self::Git, Kind::Git { .. })
                | (Self::Local, Kind::Local)
        )
    }
}

impl PartialEq<SimpleKind> for Kind {
    fn eq(&self, other: &SimpleKind) -> bool {
        other == self
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// A simplified version of the [`Origin`].
pub struct SimpleOrigin {
    url: InternedUrl,
    kind: SimpleKind,
}

impl SimpleOrigin {
    /// Create a new [`SimpleOrigin`].
    fn new(url: InternedUrl, kind: SimpleKind) -> Self {
        Self { url, kind }
    }

    /// Create a new [`SimpleOrigin`] for a registry.
    pub fn for_registry(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), SimpleKind::Registry)
    }

    /// Create a new [`SimpleOrigin`] for a git with a commit.
    pub fn for_git(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), SimpleKind::Git)
    }

    /// Create a new [`SimpleOrigin`] for a git with a commit.
    pub fn for_local(root: &Path) -> QuackResult<Self> {
        let url = Url::from_file_path(root)
            .map_err(|_| qp_err!("failed to convert the path `{}` into a url", root.display()))?;
        Ok(Self::new(url.into(), SimpleKind::Local))
    }

    /// Get an [`InternedUrl`] of this [`SimpleOrigin`].
    pub fn url(&self) -> InternedUrl {
        self.url
    }

    /// Get a [`Kind`] of this [`SimpleOrigin`].
    pub fn kind(&self) -> SimpleKind {
        self.kind
    }
}

impl PartialEq<Origin> for SimpleOrigin {
    fn eq(&self, other: &Origin) -> bool {
        self.kind() == other.kind() && self.url() == other.url()
    }
}

impl PartialEq<SimpleOrigin> for Origin {
    fn eq(&self, other: &SimpleOrigin) -> bool {
        other == self
    }
}

impl Display for SimpleOrigin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}+{}", self.kind, self.url)
    }
}

impl FromStr for SimpleOrigin {
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
            "registry" => SimpleKind::Registry,
            "local" => SimpleKind::Local,
            "git" => SimpleKind::Git,
            kind => return Err(unexpected_kind_error(kind)),
        };
        let url = Url::parse(url).with_context(|| format!("`{url}` is not a valid url"))?;
        if kind.is_local() && !url.is_local_file() {
            qp_bail!(
                "inferred kind=`local`, but url scheme is `{}`",
                url.scheme()
            )
        }
        Ok(Self::new(url.into(), kind))
    }
}

impl Serialize for SimpleOrigin {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for SimpleOrigin {
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

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
/// A simplified version of the [`Identity`].
pub struct SimpleIdentity {
    name: StrId,
    origin: SimpleOrigin,
}

impl SimpleIdentity {
    /// Create a new [`SimpleIdentity`].
    pub fn new(name: StrId, origin: SimpleOrigin) -> Self {
        Self { name, origin }
    }

    /// Get the name.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Get the [`SimpleOrigin`].
    pub fn origin(&self) -> SimpleOrigin {
        self.origin
    }
}

impl PartialEq<Identity> for SimpleIdentity {
    fn eq(&self, other: &Identity) -> bool {
        self.name() == other.name() && self.origin() == other.origin()
    }
}

impl PartialEq<SimpleIdentity> for Identity {
    fn eq(&self, other: &SimpleIdentity) -> bool {
        other == self
    }
}

impl Display for SimpleIdentity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.name, self.origin)
    }
}

impl FromStr for SimpleIdentity {
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

impl Serialize for SimpleIdentity {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for SimpleIdentity {
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

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;

    #[test]
    fn origin_display() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_registry(url);
            assert_eq!(origin.to_string(), "registry+https://localhost:9001/");
        }

        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_git(url);
            assert_eq!(origin.to_string(), "git+https://localhost:9001/");
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = SimpleOrigin::for_local(&root).unwrap();
            assert_eq!(origin.to_string(), "local+file:///tmp");
        }
    }

    #[test]
    fn origin_parse() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_registry(url);
            let formatted = origin.to_string();
            let parsed = formatted.parse::<SimpleOrigin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_git(url);
            let formatted = origin.to_string();
            let parsed = formatted.parse::<SimpleOrigin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = SimpleOrigin::for_local(&root).unwrap();
            let formatted = origin.to_string();
            let parsed = formatted.parse::<SimpleOrigin>().unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let unknown_kind = "foo+https://localhost:9001/";
            let err = unknown_kind.parse::<SimpleOrigin>().unwrap_err();
            assert_eq!(err.to_string(), "inferred unsupported kind=`foo`")
        }

        {
            let local_wrong_scheme = "local+https://localhost:9001/";
            let err = local_wrong_scheme.parse::<SimpleOrigin>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`local`, but url scheme is `https`"
            )
        }

        {
            let wrong_format = "https://localhost:9001/";
            let err = wrong_format.parse::<SimpleOrigin>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected input to be in format `kind+url`, but found `https://localhost:9001/`"
            )
        }
    }

    #[test]
    fn identity_display() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_registry(url);
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo registry+https://localhost:9001/");
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_git(url);
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo git+https://localhost:9001/");
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = SimpleOrigin::for_local(&root).unwrap();
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo local+file:///tmp");
        }
    }

    #[test]
    fn identity_parse() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_registry(url);
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<SimpleIdentity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = SimpleOrigin::for_git(url);
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<SimpleIdentity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = SimpleOrigin::for_local(&root).unwrap();
            let identity = SimpleIdentity::new("foo".into(), origin);
            let formatted = identity.to_string();
            let parsed = formatted.parse::<SimpleIdentity>().unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let invalid_format = "foo";
            let err = invalid_format.parse::<SimpleIdentity>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected a simple identity in format `<name> <origin>`"
            )
        }

        {
            let invalid_origin_format = "foo local+";
            let err = invalid_origin_format.parse::<SimpleIdentity>().unwrap_err();
            assert_eq!(
                err.to_string(),
                "simple identity `foo local+` has invalid origin syntax
`` is not a valid url
relative URL without a base"
            )
        }
    }
}
