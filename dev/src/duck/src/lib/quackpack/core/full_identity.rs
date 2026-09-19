//! A full package identity.
//!
//! This type is a suptype of [`Identity`], with additional information.
//! Note, that uniqueness of [`FullIdentity`] (which is _not_ required) does not imply uniqueness of
//! [`Identity`]: many [`FullIdentity`]ies can map to the same [`Identity`].
//!
//! For differences between [`FullIdentity`] and [`Identity`], see the `readme.md` under the `core/` directory.

use std::fmt::Display;
use std::path::Path;

use serde::{Deserialize, Serialize, de};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::identity::{Identity, Kind, Origin};
use crate::quackpack::core::{GitReference, Source, SourceKind};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackResult, StrId};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deserialize, Serialize)]
/// A [`FullIdentity`], a unique package identifier in the resolved graph.
pub struct FullIdentity {
    name: StrId,
    #[serde(flatten)]
    origin: FullOrigin,
}

impl FullIdentity {
    /// Create a new [`FullIdentity`].
    pub fn new(name: StrId, origin: FullOrigin) -> Self {
        Self { name, origin }
    }

    /// Get the name.
    pub fn name(self) -> StrId {
        self.name
    }

    /// Get the [`FullOrigin`].
    pub fn origin(self) -> FullOrigin {
        self.origin
    }

    /// Convert this [`FullIdentity`] into a [`Identity`].
    pub fn as_identity(self) -> Identity {
        Identity::new(self.name(), self.origin().as_origin())
    }

    /// Generate a human-readable description of [`self`].
    pub fn descriptive_name(self) -> String {
        match self.origin.kind {
            FullKind::Registry => format!("`{}`", self.name),
            FullKind::Git { .. } => format!("cloned from `{}`", self.origin.url),
            FullKind::Local => {
                if let Ok(path) = self.origin.url.to_path_buf() {
                    format!("at the directory `{}`", path.display())
                } else {
                    format!("at the directory `{}`", self.origin.url)
                }
            }
        }
    }

    pub fn is_local(self) -> bool {
        self.origin().is_local()
    }

    pub fn is_git(self) -> bool {
        self.origin().is_git()
    }

    pub fn is_registry(self) -> bool {
        self.origin().is_registry()
    }
}

impl From<FullIdentity> for Identity {
    fn from(value: FullIdentity) -> Self {
        value.as_identity()
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// Origin of a package.
pub struct FullOrigin {
    url: InternedUrl,
    kind: FullKind,
}

impl FullOrigin {
    /// Create a new [`FullOrigin`].
    fn new(url: InternedUrl, kind: FullKind) -> Self {
        // kind = local => url.is_local_file
        debug_assert!(
            url.is_local_file() || !kind.is_local(),
            "kind=`local` should have `file://`; has kind=`{kind}` and url=`{url}`"
        );
        Self { url, kind }
    }

    /// Create a new [`FullOrigin`] for a registry.
    pub fn for_registry(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), FullKind::Registry)
    }

    /// Create a new [`FullOrigin`] for a git with a commit.
    pub fn for_git(url: impl Into<InternedUrl>, commit: impl Into<StrId>) -> Self {
        Self::new(
            url.into(),
            FullKind::Git {
                commit: commit.into(),
            },
        )
    }

    /// Create a new [`FullOrigin`] for a git with a commit.
    pub fn for_local(root: &Path) -> QuackResult<Self> {
        let url = root.to_url()?;
        Ok(Self::new(url.into(), FullKind::Local))
    }

    /// Get an [`InternedUrl`] of this [`FullOrigin`].
    pub fn url(self) -> InternedUrl {
        self.url
    }

    /// Get a [`FullKind`] of this [`FullOrigin`].
    pub fn kind(self) -> FullKind {
        self.kind
    }

    /// Convert this [`FullOrigin`] into a [`Origin`].
    pub fn as_origin(self) -> Origin {
        Origin::new(self.url, self.kind.as_kind())
    }

    /// Check if this is a local identity.
    pub fn is_local(self) -> bool {
        matches!(self.kind(), FullKind::Local)
    }

    /// Check if this is a git identity.
    pub fn is_git(self) -> bool {
        matches!(self.kind(), FullKind::Git { commit: _ })
    }

    /// Check if this is a registry identity.
    pub fn is_registry(self) -> bool {
        matches!(self.kind(), FullKind::Registry)
    }

    /// Checks that [`self`] satisfies the requirenments of some [`Source`].
    /// This returns [`OriginSatisfiesSource`],
    /// which gives either a decisive answer or a conditional answer,
    /// which requires more work to verify.
    pub fn satisfies_source(self, source: Source) -> OriginSatisfiesSource {
        if self.url() != source.url() {
            return OriginSatisfiesSource::No;
        }
        match (self.kind(), source.kind()) {
            (FullKind::Local, SourceKind::Local) => OriginSatisfiesSource::Yes,
            (FullKind::Git { commit }, SourceKind::Git(reference)) => {
                if let GitReference::Rev(required_commit) = reference
                    && commit == *required_commit
                {
                    OriginSatisfiesSource::Yes
                } else {
                    OriginSatisfiesSource::IfGitReferencePointsToCommit {
                        url: source.url(),
                        commit,
                        reference,
                    }
                }
            }
            (FullKind::Registry, SourceKind::Registry) => OriginSatisfiesSource::Yes,
            _ => OriginSatisfiesSource::No,
        }
    }
}

/// Response to [`FullOrigin::satisfies_source`].
/// The branch [`Self::IfGitReferencePointsToCommit`] signalizes that the origin satisfies source
/// if and only if `reference` in the git repository at `url` points to `commit`.
pub enum OriginSatisfiesSource {
    Yes,
    No,
    IfGitReferencePointsToCommit {
        url: InternedUrl,
        commit: StrId,
        reference: GitReference,
    },
}

impl OriginSatisfiesSource {
    /// Detemine the conditional answer given by [`Self::IfGitReferencePointsToCommit`].
    /// This is done by performing network requests.
    ///
    /// Errors:
    /// -------
    /// We swallow errors on git fast path as this is a general way of handling them in all of the codebase,
    /// as it is well ... a fast path.
    pub async fn finish_check(self, fetcher: &Fetcher<'_>) -> QuackResult<bool> {
        match self {
            Self::Yes => Ok(true),
            Self::No => Ok(false),
            Self::IfGitReferencePointsToCommit {
                url,
                commit,
                reference,
            } => {
                let Some(fast_path_client) = fetcher.try_get_fastpath(url) else {
                    return Ok(false);
                };
                let reference_commit = match fast_path_client.get_commit_hash(reference).await {
                    Ok(commit) => commit,
                    Err(e) => {
                        fetcher.ctx().warning(e)?;
                        return Ok(false);
                    }
                };
                Ok(commit == reference_commit)
            }
        }
    }
}

impl From<FullOrigin> for Origin {
    fn from(value: FullOrigin) -> Self {
        value.as_origin()
    }
}

impl Display for FullOrigin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}+{}", self.kind, self.url)
    }
}

impl Serialize for FullOrigin {
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
            FullKind::Git { commit } => Some(commit),
            _ => None,
        };
        let helper = SerializeHelper {
            commit,
            source: self.to_string(),
        };
        helper.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for FullOrigin {
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
            ("registry", None) => FullKind::Registry,
            ("local", None) => FullKind::Local,
            ("git", Some(commit)) => FullKind::Git {
                commit: commit.into(),
            },

            // Errors,
            ("registry", Some(_)) => return Err(unexpected_commit_error("registry")),
            ("local", Some(_)) => return Err(unexpected_commit_error("local")),
            ("git", None) => return Err(expected_commit_error("git")),
            (kind, _) => return Err(unexpected_kind_error(kind)),
        };
        let url = url.to_url().map_err(de::Error::custom)?;
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

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// An [`FullOrigin`]'s kind.
pub enum FullKind {
    /// A registry-based identity.
    Registry,
    /// A git-based identity.
    Git { commit: StrId },
    /// A local-based identity.
    Local,
}

impl FullKind {
    /// Get a human-like display.
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Registry => "registry",
            Self::Git { .. } => "git",
            Self::Local => "local",
        }
    }

    /// Check, whether this [`FullKind`] is a registry kind.
    pub fn is_registry(self) -> bool {
        matches!(self, FullKind::Registry)
    }

    /// Check, whether this [`FullKind`] is a git kind.
    pub fn is_git(self) -> bool {
        matches!(self, FullKind::Git { .. })
    }

    /// Check, whether this [`FullKind`] is a local kind.
    pub fn is_local(self) -> bool {
        matches!(self, FullKind::Local)
    }

    /// Convert this [`FullKind`] into a [`Kind`].
    pub fn as_kind(self) -> Kind {
        match self {
            Self::Registry => Kind::Registry,
            Self::Git { .. } => Kind::Git,
            Self::Local => Kind::Local,
        }
    }
}

impl From<FullKind> for Kind {
    fn from(value: FullKind) -> Self {
        value.as_kind()
    }
}

impl Display for FullKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
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
            let origin = FullOrigin::for_registry(url);
            assert_eq!(origin.to_string(), "registry+https://localhost:9001/");
        }

        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_git(url, "1");
            assert_eq!(origin.to_string(), "git+https://localhost:9001/");
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = FullOrigin::for_local(&root).unwrap();
            #[cfg(windows)]
            assert_eq!(origin.to_string(), "local+file:///C:/");
            #[cfg(not(windows))]
            assert_eq!(origin.to_string(), "local+file:///tmp");
        }
    }

    #[test]
    fn origin_serialize() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_registry(url);
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            assert_eq!(
                formatted,
                r#"{
  "source": "registry+https://localhost:9001/"
}"#
            );
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_git(url, "1");
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
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = FullOrigin::for_local(&root).unwrap();
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            #[cfg(windows)]
            assert_eq!(
                formatted,
                r#"{
  "source": "local+file:///C:/"
}"#
            );
            #[cfg(not(windows))]
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
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_registry(url);
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<FullOrigin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_git(url, "1");
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<FullOrigin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = FullOrigin::for_local(&root).unwrap();
            let formatted = serde_json::to_string_pretty(&origin).unwrap();
            let parsed = serde_json::from_str::<FullOrigin>(&formatted).unwrap();
            assert_eq!(parsed, origin);
        }
        {
            let extra_commit = r#"{
  "source": "registry+https://localhost:9001/",
  "commit": "1"
}"#;
            let err = serde_json::from_str::<FullOrigin>(extra_commit).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`registry`, but `commit` was present"
            )
        }
        {
            let missing_commit = r#"{
  "source": "git+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<FullOrigin>(missing_commit).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`git`, but `commit` was not present"
            )
        }
        {
            let unknown_kind = r#"{
  "source": "foo+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<FullOrigin>(unknown_kind).unwrap_err();
            assert_eq!(err.to_string(), "inferred unsupported kind=`foo`")
        }

        {
            let local_wrong_scheme = r#"{
  "source": "local+https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<FullOrigin>(local_wrong_scheme).unwrap_err();
            assert_eq!(
                err.to_string(),
                "inferred kind=`local`, but url scheme is `https`"
            )
        }

        {
            let wrong_format = r#"{
  "source": "https://localhost:9001/"
}"#;
            let err = serde_json::from_str::<FullOrigin>(wrong_format).unwrap_err();
            assert_eq!(
                err.to_string(),
                "expected `source` to be in format `kind+url`, but found `https://localhost:9001/`"
            )
        }
    }

    #[test]
    fn identity_serialize() {
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_registry(url);
            let identity = FullIdentity::new("foo".into(), origin);
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
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_git(url, "1");
            let identity = FullIdentity::new("foo".into(), origin);
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
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = FullOrigin::for_local(&root).unwrap();
            let identity = FullIdentity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            #[cfg(windows)]
            assert_eq!(
                formatted,
                r#"{
  "name": "foo",
  "source": "local+file:///C:/"
}"#
            );
            #[cfg(not(windows))]
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
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_registry(url);
            let identity = FullIdentity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<FullIdentity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = FullOrigin::for_git(url, "1");
            let identity = FullIdentity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<FullIdentity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
        {
            #[cfg(windows)]
            let root = PathBuf::from("C:\\");
            #[cfg(not(windows))]
            let root = PathBuf::from("/tmp");
            let origin = FullOrigin::for_local(&root).unwrap();
            let identity = FullIdentity::new("foo".into(), origin);
            let formatted = serde_json::to_string_pretty(&identity).unwrap();
            let parsed = serde_json::from_str::<FullIdentity>(&formatted).unwrap();
            assert_eq!(parsed, identity);
        }
    }
}
