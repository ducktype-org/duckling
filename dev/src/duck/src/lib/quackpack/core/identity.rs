//! A package identity, a unique identifier in the dependencies' graph.

use std::fmt::Display;
use std::path::Path;

use serde::{Deserialize, Serialize, de};
use url::Url;

use crate::quackpack::core::simple_identity::{SimpleIdentity, SimpleKind, SimpleOrigin};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::{QuackResult, QuackResultContext, StrId, qp_err};

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// An [`Origin`]'s kind.
pub enum Kind {
    /// A registry-based identity.
    Registry,
    /// A git-based identity.
    Git { commit: StrId },
    /// A local-based identity.
    Local,
}

impl Kind {
    /// Get a human-like display.
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Registry => "registry",
            Self::Git { .. } => "git",
            Self::Local => "local",
        }
    }

    /// Check, whether this [`Kind`] is a registry kind.
    pub fn is_registry(&self) -> bool {
        matches!(self, Kind::Registry)
    }

    /// Check, whether this [`Kind`] is a git kind.
    pub fn is_git(&self) -> bool {
        matches!(self, Kind::Git { .. })
    }

    /// Check, whether this [`Kind`] is a local kind.
    pub fn is_local(&self) -> bool {
        matches!(self, Kind::Local)
    }

    /// Convert this [`Kind`] into a [`SimpleKind`].
    pub fn as_simple(&self) -> SimpleKind {
        match self {
            Self::Registry => SimpleKind::Registry,
            Self::Git { .. } => SimpleKind::Git,
            Self::Local => SimpleKind::Local,
        }
    }
}

impl From<Kind> for SimpleKind {
    fn from(value: Kind) -> Self {
        value.as_simple()
    }
}

impl Display for Kind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// Origin of a package.
pub struct Origin {
    url: InternedUrl,
    kind: Kind,
}

impl Origin {
    /// Create a new [`Origin`].
    fn new(url: InternedUrl, kind: Kind) -> Self {
        Self { url, kind }
    }

    /// Create a new [`Origin`] for a registry.
    pub fn for_registry(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), Kind::Registry)
    }

    /// Create a new [`Origin`] for a git with a commit.
    pub fn for_git(url: impl Into<InternedUrl>, commit: impl Into<StrId>) -> Self {
        Self::new(
            url.into(),
            Kind::Git {
                commit: commit.into(),
            },
        )
    }

    /// Create a new [`Origin`] for a git with a commit.
    pub fn for_local(root: &Path) -> QuackResult<Self> {
        let url = Url::from_file_path(root)
            .map_err(|_| qp_err!("failed to convert the path `{}` into a url", root.display()))?;
        Ok(Self::new(url.into(), Kind::Local))
    }

    /// Get an [`InternedUrl`] of this [`Origin`].
    pub fn url(&self) -> InternedUrl {
        self.url
    }

    /// Get a [`Kind`] of this [`Origin`].
    pub fn kind(&self) -> Kind {
        self.kind
    }

    /// Convert this [`Origin`] into a [`SimpleOrigin`].
    pub fn as_simple(&self) -> SimpleOrigin {
        SimpleOrigin::new(self.url, self.kind.as_simple())
    }
}

impl From<Origin> for SimpleOrigin {
    fn from(value: Origin) -> Self {
        value.as_simple()
    }
}

impl Display for Origin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}+{}", self.kind, self.url)
    }
}

impl Serialize for Origin {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        #[derive(Serialize)]
        struct SerializeHelper {
            source: String,
            #[serde(skip_serializing_if = "Option::is_none")]
            commit: Option<StrId>,
        }
        let commit = match self.kind {
            Kind::Git { commit } => Some(commit),
            _ => None,
        };
        let helper = SerializeHelper {
            commit,
            source: self.to_string(),
        };
        helper.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for Origin {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct DeserializeHelper {
            source: String,
            commit: Option<String>,
        }
        let helper = DeserializeHelper::deserialize(deserializer)?;
        let Some((kind, url)) = helper.source.split_once('+') else {
            let msg = format!(
                "expected `source` to be in format `kind+url`, but found `{}`",
                helper.source
            );
            return Err(de::Error::custom(msg));
        };
        let unexpected_commit_error = |kind: &str| {
            let msg = format!("inferred kind=`{kind}`, but `commit` was present");
            de::Error::custom(msg)
        };

        let expected_commit_error = |kind: &str| {
            let msg = format!("inferred kind=`{kind}`, but `commit` was not present");
            de::Error::custom(msg)
        };

        let unexpected_kind_error = |kind: &str| {
            let msg = format!("inferred unsupported kind=`{kind}`");
            de::Error::custom(msg)
        };
        let kind = match (kind, helper.commit.as_deref()) {
            ("registry", None) => Kind::Registry,
            ("local", None) => Kind::Local,
            ("git", Some(commit)) => Kind::Git {
                commit: commit.into(),
            },

            // Errors,
            ("registry", Some(_)) => return Err(unexpected_commit_error("registry")),
            ("local", Some(_)) => return Err(unexpected_commit_error("local")),
            ("git", None) => return Err(expected_commit_error("git")),
            (kind, _) => return Err(unexpected_kind_error(kind)),
        };
        let url = Url::parse(url)
            .with_context(|| format!("`{url}` is not a valid url"))
            .map_err(de::Error::custom)?;
        if kind.is_local() && !url.is_local_file() {
            let msg = format!(
                "inferred kind=`local`, but url scheme is `{}`",
                url.scheme()
            );
            return Err(de::Error::custom(msg));
        }
        Ok(Self::new(url.into(), kind))
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deserialize, Serialize)]
/// An [`Identity`], a unique package identifier in the resolved graph.
pub struct Identity {
    name: StrId,
    #[serde(flatten)]
    origin: Origin,
}

impl Identity {
    /// Create a new [`Identity`].
    pub fn new(name: StrId, origin: Origin) -> Self {
        Self { name, origin }
    }

    /// Get the name.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Get the [`Origin`].
    pub fn origin(&self) -> Origin {
        self.origin
    }

    /// Convert this [`Identity`] into a [`SimpleIdentity`].
    pub fn as_simple(&self) -> SimpleIdentity {
        SimpleIdentity::new(self.name(), self.origin().as_simple())
    }
}

impl From<Identity> for SimpleIdentity {
    fn from(value: Identity) -> Self {
        value.as_simple()
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
            let origin = Origin::for_registry(url);
            assert_eq!(origin.to_string(), "registry+https://localhost:9001/");
        }

        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_git(url, "1");
            assert_eq!(origin.to_string(), "git+https://localhost:9001/");
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            assert_eq!(origin.to_string(), "local+file:///tmp");
        }
    }

    #[test]
    fn origin_serialize() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_registry(url);
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "source": "registry+https://localhost:9001/"
}"#
            );
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_git(url, "1");
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "source": "git+https://localhost:9001/",
  "commit": "1"
}"#
            );
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "source": "local+file:///tmp"
}"#
            );
        }
    }

    #[test]
    fn origin_deserialize() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_registry(url);
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<Origin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_git(url, "1");
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<Origin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<Origin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let extra_commit = r#"{
  "source": "registry+https://localhost:9001/",
  "commit": "1"
}"#;
            let err = serde_json::from_str::<Origin>(extra_commit).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`registry`, but `commit` was present"
            )
        }
        {
            let missing_commit = r#"{
  "source": "git+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<Origin>(missing_commit).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`git`, but `commit` was not present"
            )
        }
        {
            let unknown_kind = r#"{
  "source": "foo+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<Origin>(unknown_kind).unwrap_err();
            assert_eq!(err.to_string(), "inferred unsupported kind=`foo`")
        }

        {
            let local_wrong_scheme = r#"{
  "source": "local+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<Origin>(local_wrong_scheme).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`local`, but url scheme is `https`"
            )
        }

        {
            let wrong_format = r#"{
  "source": "https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<Origin>(wrong_format).unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected `source` to be in format `kind+url`, but found `https://localhost:9001/`"
            )
        }
    }

    #[test]
    fn identity_serialize() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_registry(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "name": "foo",
  "source": "registry+https://localhost:9001/"
}"#
            );
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_git(url, "1");
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "name": "foo",
  "source": "git+https://localhost:9001/",
  "commit": "1"
}"#
            );
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "name": "foo",
  "source": "local+file:///tmp"
}"#
            );
        }
    }

    #[test]
    fn identity_deserialize() {
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_registry(url);
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<Identity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let url = Url::parse("https://localhost:9001").unwrap();
            let origin = Origin::for_git(url, "1");
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<Identity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let root = PathBuf::from("/tmp");
            let origin = Origin::for_local(&root).unwrap();
            let identity = Identity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<Identity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
    }
}
