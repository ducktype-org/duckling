// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::{Path, PathBuf};

use crate::StrId;
use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::short_name_for_host;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::with_version::WithVersion;

/// Unique identifier of a package, used throughout the whole project.
pub type PackageId = WithVersion<FullIdentity>;

impl PackageId {
    /// Get the [`FullIdentity`] of the package.
    pub fn identity(self) -> FullIdentity {
        *self.value()
    }

    /// Get the name of the package.
    pub fn name(self) -> StrId {
        self.identity().name()
    }

    /// Get the [`FullOrigin`] of the package.
    pub fn origin(self) -> FullOrigin {
        self.identity().origin()
    }

    /// Get the url of the package.
    pub fn url(self) -> InternedUrl {
        self.origin().url()
    }

    /// Get the package's kind.
    pub fn kind(self) -> FullKind {
        self.origin().kind()
    }

    #[track_caller]
    /// Push the unique path to the base, to get the path where the package should be stored.
    ///
    /// They correspond to paths under `pkg/`, described in [`storage::paths`].
    ///
    /// [`storage::paths`]: crate::quackpack::core::storage::paths
    pub fn push_storage_path(self, root: &Path) -> PathBuf {
        match self.kind() {
            FullKind::Registry => {
                let name = format!("registry-{}", short_name_for_host(&self.url()));
                root.join(name)
                    .join(format!("{}-{}", self.name(), self.version()))
            }
            FullKind::Git { commit } => root
                .join("git")
                .join(short_name_for_host(&self.url()))
                .join(commit),
            FullKind::Local => unreachable!("local packages are not stored in storage {self:?}"),
        }
    }
}

impl From<PackageId> for WithVersion<Identity> {
    fn from(pkg_id: PackageId) -> Self {
        Self::new(pkg_id.identity().into(), pkg_id.version())
    }
}
