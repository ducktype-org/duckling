//! A general package abstraction.
use std::fmt;
use std::path::{Path, PathBuf};

use crate::QuackResult;
use crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::{Dependencies, Manifest, Profiles};
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;

pub enum PackageType {
    Package,
    Frontmatter,
}

pub trait AsPackage: fmt::Debug + Send + Sync {
    fn manifest(&self) -> &Manifest;
    fn root(&self) -> &Path;
    fn is_global(&self) -> bool;
    fn kind(&self) -> PackageType;
    fn get_package(&self) -> Option<&Package>;
    fn get_frontmatter(&self) -> Option<&FrontMatterScript>;

    fn unwrap_package(&self) -> &Package {
        self.get_package().unwrap()
    }

    fn unwrap_frontmatter(&self) -> &FrontMatterScript {
        self.get_frontmatter().unwrap()
    }

    fn as_a_local_identity(&self) -> QuackResult<Identity>;
}

#[derive(Clone, Debug)]
/// High-level abstraction over a package we are currently working on.
pub struct Package {
    original_content: String,
    original_schema: ManifestSchema,
    manifest: Manifest,
    root: PathBuf,
    manifest_path: PathBuf,
    artifacts_dir: ArtifactsLayout,
    source_dir: PathBuf,
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
            original_content,
            original_schema,
            manifest,
            root,
            manifest_path,
            artifacts_dir,
            source_dir: source_directory,
        }
    }

    /// Get the original manifest file content.
    pub fn original_content(&self) -> &str {
        &self.original_content
    }

    /// Get the original parsed manifest schema.
    pub fn original_schema(&self) -> &ManifestSchema {
        &self.original_schema
    }

    /// Get the high-level abstraction over the manifest.
    pub fn manifest(&self) -> &Manifest {
        &self.manifest
    }

    /// Get the root directory of the package.
    pub fn root_directory(&self) -> &Path {
        &self.root
    }

    /// Get the path to the source directory.
    pub fn source_directory(&self) -> &Path {
        &self.source_dir
    }

    /// Get the path to the manifest file.
    pub fn manifest_path(&self) -> &Path {
        &self.manifest_path
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        &self.artifacts_dir
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

impl AsPackage for Package {
    fn manifest(&self) -> &Manifest {
        self.manifest()
    }

    fn root(&self) -> &Path {
        self.root_directory()
    }

    fn is_global(&self) -> bool {
        self.is_global()
    }

    fn kind(&self) -> PackageType {
        PackageType::Package
    }

    fn get_package(&self) -> Option<&Package> {
        Some(self)
    }

    fn get_frontmatter(&self) -> Option<&FrontMatterScript> {
        None
    }

    fn as_a_local_identity(&self) -> QuackResult<Identity> {
        self.as_a_local_identity()
    }
}

#[derive(Debug)]
pub struct FrontMatterScript {
    path: PathBuf,
    original_schema: ManifestSchema,
    manifest: Manifest,
}

impl FrontMatterScript {
    /// Create a new [`FrontMatterScript`].
    pub fn new(path: PathBuf, original_schema: ManifestSchema, manifest: Manifest) -> Self {
        Self {
            path,
            original_schema,
            manifest,
        }
    }

    /// Get the path of the script.
    pub fn script_file(&self) -> &Path {
        &self.path
    }

    /// Get the schema of the script's frontmatter.
    pub fn original_schema(&self) -> &ManifestSchema {
        &self.original_schema
    }

    /// Get the manifest constructed from the script's frontmatter.
    pub fn manifest(&self) -> &Manifest {
        &self.manifest
    }

    /// Get the dependencies specified in the frontmatter.
    pub fn dependencies(&self) -> &Dependencies {
        self.manifest().dependencies()
    }

    /// Get the dev-dependencies specified in the frontmatter.
    pub fn dev_dependencies(&self) -> &Dependencies {
        self.manifest().dev_dependencies()
    }

    /// Get the profiles specified in the frontmatter.
    pub fn profiles(&self) -> &Profiles {
        self.manifest().profiles()
    }

    /// Convert this package to a [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        let origin = Origin::for_local(self.script_file())?;
        Ok(Identity::new(self.manifest().name(), origin))
    }
}

impl AsPackage for FrontMatterScript {
    fn manifest(&self) -> &Manifest {
        self.manifest()
    }

    fn root(&self) -> &Path {
        self.script_file()
    }

    fn is_global(&self) -> bool {
        false
    }

    fn kind(&self) -> PackageType {
        PackageType::Frontmatter
    }

    fn get_package(&self) -> Option<&Package> {
        None
    }

    fn get_frontmatter(&self) -> Option<&FrontMatterScript> {
        Some(self)
    }

    fn as_a_local_identity(&self) -> QuackResult<Identity> {
        self.as_a_local_identity()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assert_send_sync_package_and_frontmatter() {
        fn assert_send<T: Send>() {}
        fn assert_sync<T: Sync>() {}
        assert_send::<Package>();
        assert_sync::<Package>();
        assert_send::<FrontMatterScript>();
        assert_sync::<FrontMatterScript>();
    }
}
