//! A general package abstraction.
use std::fmt;
use std::path::{Path, PathBuf};

use super::compile::artifacts_layout::ArtifactsLayout;
use super::identity::{Identity, Origin};
use super::script::Script;
use super::{Manifest, VenvConfig, Version};
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::{QuackResult, StrId};

#[derive(Clone, Debug)]
/// Entities which can be treated as a package (have their own venvs).
pub enum AnyPackage {
    Package(Package),
    Script(Script),
}

impl AnyPackage {
    /// Get the high-level abstraction over the manifest.
    pub fn manifest(&self) -> &Manifest {
        match self {
            Self::Package(package) => package.manifest(),
            Self::Script(script) => script.manifest(),
        }
    }

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        match self {
            Self::Package(package) => package.into_manifest(),
            Self::Script(script) => script.into_manifest(),
        }
    }

    /// Get the root directory of the package / path of the script.
    pub fn root(&self) -> &Path {
        match self {
            Self::Package(package) => package.root_directory(),
            Self::Script(script) => script.script_path(),
        }
    }

    /// Get path to the source directory.
    pub fn src(&self) -> Option<&Path> {
        match self {
            Self::Package(package) => package.source_directory(),
            Self::Script(_) => None,
        }
    }

    /// Check if this is the global package.
    pub fn is_global(&self) -> bool {
        match self {
            Self::Package(package) => package.is_global(),
            Self::Script(_) => false,
        }
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &Path {
        match self {
            Self::Package(package) => package.artifacts_directory(),
            Self::Script(script) => script.artifacts_directory(),
        }
    }

    /// Get the artifacts layout.
    pub fn artifacts_layout<T: ArtifactsLayout>(&self) -> T {
        match self {
            Self::Package(package) => package.artifacts_layout(),
            Self::Script(script) => script.artifacts_layout(),
        }
    }

    /// Convert this package to an [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        match self {
            Self::Package(package) => package.as_a_local_identity(),
            Self::Script(script) => script.as_a_local_identity(),
        }
    }

    /// Try to cast `&self` into `&Package`.
    pub fn try_get_package(&self) -> Option<&Package> {
        match self {
            Self::Package(package) => Some(package),
            Self::Script(_) => None,
        }
    }

    /// Try to cast `&self` into `&Script`.
    pub fn try_get_script(&self) -> Option<&Script> {
        match self {
            Self::Package(_) => None,
            Self::Script(script) => Some(script),
        }
    }

    /// Cast `&self` into `&Package` and panic on mismatch.
    #[track_caller]
    pub fn get_package(&self) -> &Package {
        match self {
            Self::Package(package) => package,
            Self::Script(_) => {
                panic!("tried to cast `AnyPackage` with a script to a package")
            }
        }
    }

    /// Cast `&self` into `&Script` and panic on mismatch.
    #[track_caller]
    pub fn get_script(&self) -> &Script {
        match self {
            Self::Package(_) => {
                panic!("tried to cast `AnyPackage` with a package to a script")
            }
            Self::Script(script) => script,
        }
    }

    /// Try to extract [`Package`] from `self`.
    pub fn try_into_package(self) -> Option<Package> {
        match self {
            Self::Package(package) => Some(package),
            Self::Script(_) => None,
        }
    }

    /// Try to extract [`Script`] from `self`.
    pub fn try_into_script(self) -> Option<Script> {
        match self {
            Self::Package(_) => None,
            Self::Script(script) => Some(script),
        }
    }

    /// Extract [`Package`] from `self` and panic on mismatch.
    #[track_caller]
    pub fn unwrap_package(self) -> Package {
        match self {
            Self::Package(package) => package,
            Self::Script(_) => {
                panic!("tried to cast `AnyPackage` with a script to a package")
            }
        }
    }

    /// Extract [`Script`] from `self` and panic on mismatch.
    #[track_caller]
    pub fn unwrap_script(self) -> Script {
        match self {
            Self::Package(_) => {
                panic!("tried to cast `AnyPackage` with a package to a script")
            }
            Self::Script(script) => script,
        }
    }

    /// Check if this package is a _real_ package.
    pub fn is_package(&self) -> bool {
        matches!(self, AnyPackage::Package(..))
    }

    /// Check if this package is a script.
    pub fn is_script(&self) -> bool {
        matches!(self, AnyPackage::Script(..))
    }

    pub fn name(&self) -> StrId {
        self.manifest().name()
    }

    pub fn version(&self) -> Version {
        self.manifest().version()
    }

    pub fn venv(&self) -> &VenvConfig {
        self.manifest().venv()
    }
}

#[derive(Clone)]
/// High-level abstraction over a package we are currently working on.
pub struct Package {
    /// Raw (not deserialized) contents of the manifest.
    original_content: String,
    /// Original schema of the manifest.
    original_schema: ManifestSchema,
    /// Manifest of the package.
    manifest: Manifest,
    /// Path to the root folder of the package.
    root: PathBuf,
    /// Path to the manifest of the package.
    manifest_path: PathBuf,
    /// Where the build artifacts should be located.
    artifacts_dir: PathBuf,
    /// Path to the source code folder of the package.
    possible_source_dir: PathBuf,
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
            original_content,
            original_schema,
            manifest,
            root,
            manifest_path,
            artifacts_dir: artifacts_dir.clone(),
            possible_source_dir: source_directory,
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

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        self.manifest
    }

    /// Get the root directory of the package.
    pub fn root_directory(&self) -> &Path {
        &self.root
    }

    /// Get the path to the source directory.
    /// Global package has no src folder, so in that case this function returns [`None`].
    pub fn source_directory(&self) -> Option<&Path> {
        if self.is_global() {
            None
        } else {
            Some(&self.possible_source_dir)
        }
    }

    /// Get the path to the manifest file.
    pub fn manifest_path(&self) -> &Path {
        &self.manifest_path
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &Path {
        &self.artifacts_dir
    }

    /// Get the the artifacts layout.
    pub fn artifacts_layout<T: ArtifactsLayout>(&self) -> T {
        T::new(self.artifacts_dir.clone())
    }

    /// Convert this package to an [`Identity`].
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

impl fmt::Debug for Package {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Package")
            .field("manifest", &self.manifest)
            .field("root", &self.root)
            .field("manifest_path", &self.manifest_path)
            .field("artifacts_dir", &self.artifacts_dir)
            .field("possible_source_dir", &self.possible_source_dir)
            .finish_non_exhaustive()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assert_send_sync_package_and_script() {
        fn assert_send<T: Send>() {}
        fn assert_sync<T: Sync>() {}
        assert_send::<Package>();
        assert_sync::<Package>();
        assert_send::<Script>();
        assert_sync::<Script>();
        assert_send::<AnyPackage>();
        assert_sync::<AnyPackage>();
    }
}
