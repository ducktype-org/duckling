//! A general package abstraction.
use std::ffi::{OsStr, OsString};
use std::fmt;
use std::path::{Path, PathBuf};

use crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::{Dependencies, Manifest, Profiles, Version};
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::{QuackResult, QuackResultContext, StrId};

#[derive(Clone, Debug)]
/// Entities which can be treated as a package (have their own venvs).
pub enum AnyPackage {
    Package(Package),
    Frontmatter(FrontMatterScript),
}

impl AnyPackage {
    /// Get the high-level abstraction over the manifest.
    pub fn manifest(&self) -> &Manifest {
        match self {
            Self::Package(package) => package.manifest(),
            Self::Frontmatter(frontmatter) => frontmatter.manifest(),
        }
    }

    /// Get the root directory of the package / path of the script.
    pub fn root(&self) -> &Path {
        match self {
            Self::Package(package) => package.root_directory(),
            Self::Frontmatter(frontmatter) => frontmatter.script_file(),
        }
    }

    /// Get path to the source directory.
    pub fn src(&self) -> Option<&Path> {
        match self {
            Self::Package(package) => package.source_directory(),
            Self::Frontmatter(_) => None,
        }
    }

    /// Check if this is the global package.
    pub fn is_global(&self) -> bool {
        match self {
            Self::Package(package) => package.is_global(),
            Self::Frontmatter(_) => false,
        }
    }

    /// Get the artifacts layout.
    pub fn artifacts_dir(&self) -> &ArtifactsLayout {
        match self {
            Self::Package(package) => package.artifacts_directory(),
            Self::Frontmatter(frontmatter_script) => frontmatter_script.artifacts_directory(),
        }
    }

    /// Check if this is the global package.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        match self {
            Self::Package(package) => package.as_a_local_identity(),
            Self::Frontmatter(frontmatter) => frontmatter.as_a_local_identity(),
        }
    }

    /// Try to cast `&self` into `&Package`.
    pub fn try_get_package(&self) -> Option<&Package> {
        match self {
            Self::Package(package) => Some(package),
            Self::Frontmatter(_) => None,
        }
    }

    /// Try to cast `&self` into `&FrontMatterScript`.
    pub fn try_get_frontmatter(&self) -> Option<&FrontMatterScript> {
        match self {
            Self::Package(_) => None,
            Self::Frontmatter(frontmatter) => Some(frontmatter),
        }
    }

    /// Cast `&self` into `&Package` and panic on mismatch.
    pub fn get_package(&self) -> &Package {
        match self {
            Self::Package(package) => package,
            Self::Frontmatter(_) => {
                panic!("tried to cast `AnyPackage` with a frontmatter to a package")
            }
        }
    }

    /// Cast `&self` into `&FrontMatterScript` and panic on mismatch.
    pub fn get_frontmatter(&self) -> &FrontMatterScript {
        match self {
            Self::Package(_) => {
                panic!("tried to cast `AnyPackage` with a package to a frontmatter")
            }
            Self::Frontmatter(frontmatter) => frontmatter,
        }
    }

    /// Try to extract [`Package`] from `self`.
    pub fn try_into_package(self) -> Option<Package> {
        match self {
            Self::Package(package) => Some(package),
            Self::Frontmatter(_) => None,
        }
    }

    /// Try to extract [`FrontMatterScript`] from `self`.
    pub fn try_into_frontmatter(self) -> Option<FrontMatterScript> {
        match self {
            Self::Package(_) => None,
            Self::Frontmatter(frontmatter) => Some(frontmatter),
        }
    }

    /// Extract [`Package`] from `self` and panic on mismatch.
    pub fn unwrap_package(self) -> Package {
        match self {
            Self::Package(package) => package,
            Self::Frontmatter(_) => {
                panic!("tried to cast `AnyPackage` with a frontmatter to a package")
            }
        }
    }

    /// Extract [`FrontMatterScript`] from `self` and panic on mismatch.
    pub fn unwrap_frontmatter(self) -> FrontMatterScript {
        match self {
            Self::Package(_) => {
                panic!("tried to cast `AnyPackage` with a package to a frontmatter")
            }
            Self::Frontmatter(frontmatter) => frontmatter,
        }
    }

    /// Check if this package is a _real_ package.
    pub fn is_package(&self) -> bool {
        matches!(self, AnyPackage::Package(..))
    }

    /// Check if this package is a script's frontmatter.
    pub fn is_frontmatter(&self) -> bool {
        matches!(self, AnyPackage::Frontmatter(..))
    }

    pub fn name(&self) -> StrId {
        self.manifest().name()
    }

    pub fn version(&self) -> Version {
        self.manifest().version()
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
    artifacts_dir: ArtifactsLayout,
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
        let artifacts_dir = ArtifactsLayout::new(root.join(".duck_build"));
        let source_directory = root.join("src");
        Self {
            original_content,
            original_schema,
            manifest,
            root,
            manifest_path,
            artifacts_dir,
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
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        &self.artifacts_dir
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
            .field("artifacts_dir", &self.artifacts_dir.root_directory())
            .field("possible_source_dir", &self.possible_source_dir)
            .finish_non_exhaustive()
    }
}

#[derive(Clone)]
pub struct FrontMatterScript {
    /// Path to the script.
    path: PathBuf,
    /// The folder the script is located in.
    script_folder: PathBuf,
    /// Name of the script (a.k.a. file stem).
    script_name: OsString,
    /// Original schema of the frontmatter.
    original_schema: ManifestSchema,
    /// Manifest constructed from the frontmatter.
    manifest: Manifest,
    /// Where the build artifacts should be located.
    artifacts_dir: ArtifactsLayout,
}

impl FrontMatterScript {
    /// Create a new [`FrontMatterScript`].
    pub fn new(
        path: PathBuf,
        original_schema: ManifestSchema,
        manifest: Manifest,
    ) -> QuackResult<Self> {
        let script_folder = path
            .parent()
            .context_internal("script path without parent")?;
        let script_name = path
            .file_stem()
            .context_internal("script path without file stem")?;
        let artifacts_dir =
            ArtifactsLayout::new(script_folder.join(".duck_build").join(script_name));
        Ok(Self {
            path: path.clone(),
            script_folder: script_folder.to_path_buf(),
            script_name: script_name.to_os_string(),
            original_schema,
            manifest,
            artifacts_dir,
        })
    }

    /// Get the path of the script.
    pub fn script_file(&self) -> &Path {
        &self.path
    }

    /// Get the folder of the script.
    pub fn script_folder(&self) -> &Path {
        &self.script_folder
    }

    /// Get the name of the script.
    pub fn script_name(&self) -> &OsStr {
        &self.script_name
    }

    /// Get the schema of the script's frontmatter.
    pub fn original_schema(&self) -> &ManifestSchema {
        &self.original_schema
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        &self.artifacts_dir
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

    /// Convert this package to an [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        let origin = Origin::for_local(self.script_file())?;
        Ok(Identity::new(self.manifest().name(), origin))
    }
}

impl fmt::Debug for FrontMatterScript {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Script with a frontmatter")
            .field("manifest", &self.manifest)
            .field("script path", &self.path)
            .finish_non_exhaustive()
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
