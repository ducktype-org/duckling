//! A general package abstraction.
use crate::quackpack::{
    core::{Manifest, storage::freeze::FreezeDep},
    schemas::manifest::Manifest as ManifestSchema,
};
use std::{
    path::{Path, PathBuf},
    sync::Arc,
};

#[derive(Debug, Clone)]
/// High-level abstraction over a package we are currently working on.
pub struct Package {
    inner: Arc<PackageInner>,
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
        let artifacts_dir = root.join(".duck_build");
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
    pub fn artifacts_directory(&self) -> &Path {
        &self.inner.artifacts_dir
    }

    /// Convert this package to a [`FreezeDep`].
    pub fn as_freeze_dep(&self) -> FreezeDep {
        FreezeDep::new(self.manifest().name(), self.manifest().version())
    }

    /// Is this the global package.
    pub fn is_global(&self) -> bool {
        self.manifest().is_global()
    }
}

#[derive(Debug)]
struct PackageInner {
    original_content: String,
    original_schema: ManifestSchema,
    manifest: Manifest,
    root: PathBuf,
    manifest_path: PathBuf,
    artifacts_dir: PathBuf,
    source_dir: PathBuf,
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
