//! A general package abstraction.
use std::fmt;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use crate::QuackResult;
use crate::quackpack::core::Manifest;
use crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;

#[derive(Clone)]
/// High-level abstraction over a package we are currently working on.
pub struct Package {
    inner: Arc<PackageInner>,
}

impl fmt::Debug for Package {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        fmt::Debug::fmt(&*self.inner, f)
    }
}

impl Package {
    /// Create a new [`Package`].
    pub fn new(
        original_content: String,
        original_schema: ManifestSchema,
        manifest: Manifest,
        root: PathBuf,
        manifest_path: PathBuf,
    ) -> Self {
        let artifacts_dir = ArtifactsLayout::new(root.join(".duck_build"));
        let source_directory = root.join("src");
        Self {
            inner: Arc::new(PackageInner {
                original_content,
                original_schema,
                manifest,
                root,
                manifest_path,
                artifacts_dir,
                source_dir: source_directory,
            }),
        }
    }

    /// Get the original manifest file content.
    pub fn original_content(&self) -> &str {
        &self.inner.original_content
    }

    /// Get the original parsed manifest schema.
    pub fn original_schema(&self) -> &ManifestSchema {
        &self.inner.original_schema
    }

    /// Get the high-level abstraction over the manifest.
    pub fn manifest(&self) -> &Manifest {
        &self.inner.manifest
    }

    /// Get the root directory of the package.
    pub fn root_directory(&self) -> &Path {
        &self.inner.root
    }

    /// Get the path to the source directory.
    pub fn source_directory(&self) -> &Path {
        &self.inner.source_dir
    }

    /// Get the path to the manifest file.
    pub fn manifest_path(&self) -> &Path {
        &self.inner.manifest_path
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        &self.inner.artifacts_dir
    }

    /// Convert this package to a [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        let origin = Origin::for_local(self.root_directory())?;
        Ok(Identity::new(self.manifest().name(), origin))
    }

    /// Is this the global package.
    pub fn is_global(&self) -> bool {
        self.manifest().is_global()
    }
}

struct PackageInner {
    original_content: String,
    original_schema: ManifestSchema,
    manifest: Manifest,
    root: PathBuf,
    manifest_path: PathBuf,
    artifacts_dir: ArtifactsLayout,
    source_dir: PathBuf,
}

impl fmt::Debug for PackageInner {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Package")
            .field("manifest", &self.manifest)
            .field("root", &self.root)
            .field("manifest_path", &self.manifest_path)
            .field("artifacts_dir", &self.artifacts_dir.root_directory())
            .field("source_dir", &self.source_dir)
            .finish_non_exhaustive()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assert_send_sync_package() {
        fn assert_send<T: Send>() {}
        fn assert_sync<T: Sync>() {}
        assert_send::<Package>();
        assert_sync::<Package>();
    }
}
