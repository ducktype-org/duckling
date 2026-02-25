use crate::quackpack::{core::Manifest, schemas::manifest::Manifest as ManifestSchema};
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
    /// Create a new package.
    pub fn new(
        original_content: String,
        original_schema: ManifestSchema,
        manifest: Manifest,
        root: PathBuf,
        manifest_path: PathBuf,
    ) -> Self {
        Self {
            inner: Arc::new(PackageInner {
                original_content,
                original_schema,
                manifest,
                root,
                manifest_path,
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

    /// Get the root directory of the Package.
    pub fn root_directory(&self) -> &Path {
        &self.inner.root
    }

    pub fn manifest_path(&self) -> &Path {
        &self.inner.manifest_path
    }
}

#[derive(Debug)]
struct PackageInner {
    original_content: String,
    original_schema: ManifestSchema,
    manifest: Manifest,
    root: PathBuf,
    manifest_path: PathBuf,
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
