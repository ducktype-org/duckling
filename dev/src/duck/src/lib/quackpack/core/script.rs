//! Different versions of scripts.

use std::ffi::{OsStr, OsString};
use std::fmt;
use std::path::{Path, PathBuf};

use super::compile::artifacts_layout::ArtifactsLayout;
use super::identity::{Identity, Origin};
use super::{Dependencies, Manifest, Package, Profiles, capture_frontmatter};
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, QuackResultContext};

#[derive(Clone, Debug)]
/// A generic script.
///
/// Can be either standalone (and in that case has a frontmatter), or associated with a package.
pub enum Script {
    Standalone(StandaloneScript),
    Associated(PackageScript),
}

impl From<StandaloneScript> for Script {
    fn from(value: StandaloneScript) -> Self {
        Self::Standalone(value)
    }
}

impl From<PackageScript> for Script {
    fn from(value: PackageScript) -> Self {
        Self::Associated(value)
    }
}

impl Script {
    /// Get the path to the script.
    pub fn script_path(&self) -> &Path {
        match self {
            Self::Standalone(standalone_script) => standalone_script.frontmatter().script_file(),
            Self::Associated(package_script) => package_script.script_path(),
        }
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        match self {
            Self::Standalone(standalone_script) => standalone_script.artifacts_directory(),
            Self::Associated(package_script) => package_script.artifacts_directory(),
        }
    }

    /// Get the appropriate manifest.
    pub fn manifest(&self) -> &Manifest {
        match self {
            Self::Standalone(standalone_script) => standalone_script.manifest(),
            Self::Associated(package_script) => package_script.manifest(),
        }
    }

    /// Get the dependencies.
    pub fn dependencies(&self) -> &Dependencies {
        self.manifest().dependencies()
    }

    /// Get the dev-dependencies.
    pub fn dev_dependencies(&self) -> &Dependencies {
        self.manifest().dev_dependencies()
    }

    /// Get the profiles.
    pub fn profiles(&self) -> &Profiles {
        self.manifest().profiles()
    }

    /// Convert this script to an [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        match self {
            Self::Standalone(standalone_script) => standalone_script.as_a_local_identity(),
            Self::Associated(package_script) => package_script.as_a_local_identity(),
        }
    }

    /// Check if this script is a [`StandaloneScript`].
    pub fn is_standalone(&self) -> bool {
        matches!(self, Self::Standalone(..))
    }

    /// Check if this script is a [`PackageScript`].
    pub fn is_associated(&self) -> bool {
        matches!(self, Self::Associated(..))
    }

    /// Try to cast `&self` into `&Package`.
    pub fn try_get_associated(&self) -> Option<&PackageScript> {
        match self {
            Self::Associated(package) => Some(package),
            Self::Standalone(_) => None,
        }
    }

    /// Try to cast `&self` into `&StandaloneScript`.
    pub fn try_get_standalone(&self) -> Option<&StandaloneScript> {
        match self {
            Self::Associated(_) => None,
            Self::Standalone(script) => Some(script),
        }
    }

    /// Cast `&self` into `&PackageScript` and panic on mismatch.
    #[track_caller]
    pub fn get_associated(&self) -> &PackageScript {
        match self {
            Self::Associated(package) => package,
            Self::Standalone(_) => {
                panic!("tried to cast `Script` with a standalone script to a package script")
            }
        }
    }

    /// Cast `&self` into `&StandaloneScript` and panic on mismatch.
    #[track_caller]
    pub fn get_standalone(&self) -> &StandaloneScript {
        match self {
            Self::Associated(_) => {
                panic!("tried to cast `Script` with a package script to a standalone script")
            }
            Self::Standalone(script) => script,
        }
    }

    /// Try to extract [`PackageScript`] from `self`.
    pub fn try_into_associated(self) -> Option<PackageScript> {
        match self {
            Self::Associated(package) => Some(package),
            Self::Standalone(_) => None,
        }
    }

    /// Try to extract [`StandaloneScript`] from `self`.
    pub fn try_into_standalone(self) -> Option<StandaloneScript> {
        match self {
            Self::Associated(_) => None,
            Self::Standalone(script) => Some(script),
        }
    }

    /// Extract [`PackageScript`] from `self` and panic on mismatch.
    #[track_caller]
    pub fn unwrap_associated(self) -> PackageScript {
        match self {
            Self::Associated(package) => package,
            Self::Standalone(_) => {
                panic!("tried to cast `Script` with a standalone script to a package script")
            }
        }
    }

    /// Extract [`StandaloneScript`] from `self` and panic on mismatch.
    #[track_caller]
    pub fn unwrap_standalone(self) -> StandaloneScript {
        match self {
            Self::Associated(_) => {
                panic!("tried to cast `Script` with a package script to a standalone script")
            }
            Self::Standalone(script) => script,
        }
    }

    /// Returns true if script at `path` has a frontmatter.
    pub fn has_frontmatter(path: &Path) -> QuackResult<bool> {
        let contents = path.read_to_string()?;
        capture_frontmatter(&contents).map(|maybe_frontmatter| maybe_frontmatter.is_some())
    }

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        match self {
            Self::Standalone(standalone_script) => standalone_script.into_manifest(),
            Self::Associated(package_script) => package_script.into_manifest(),
        }
    }
}

#[derive(Clone, Debug)]
/// A script associated with a package.
pub struct PackageScript {
    package: Package,
    script_path: PathBuf,
}

impl PackageScript {
    /// Create a new [`PackageScript`].
    pub fn new(package: Package, script_path: PathBuf) -> Self {
        Self {
            package,
            script_path,
        }
    }

    /// Get the underlying [`Package`].
    pub fn package(&self) -> &Package {
        &self.package
    }

    /// Get the path to the script.
    pub fn script_path(&self) -> &Path {
        &self.script_path
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        self.package().artifacts_directory()
    }

    /// Get the package's manifest.
    pub fn manifest(&self) -> &Manifest {
        self.package().manifest()
    }

    /// Get the package's dependencies.
    pub fn dependencies(&self) -> &Dependencies {
        self.manifest().dependencies()
    }

    /// Get the package's dev-dependencies.
    pub fn dev_dependencies(&self) -> &Dependencies {
        self.manifest().dev_dependencies()
    }

    /// Get the package's profiles.
    pub fn profiles(&self) -> &Profiles {
        self.manifest().profiles()
    }

    /// Convert this script to an [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        self.package().as_a_local_identity()
    }

    /// Transform into the underlying [`Package`].
    pub fn into_package(self) -> Package {
        self.package
    }

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        self.into_package().into_manifest()
    }
}

#[derive(Clone, Debug)]
/// A standalone script, not associated with any package.
pub struct StandaloneScript {
    frontmatter: FrontMatter,
}

impl StandaloneScript {
    /// Create a new [`StandaloneScript`].
    pub fn new(frontmatter: FrontMatter) -> Self {
        Self { frontmatter }
    }

    /// Get the [`FrontMatter`] of this script.
    pub fn frontmatter(&self) -> &FrontMatter {
        &self.frontmatter
    }

    /// Get the path to the artifacts directory.
    pub fn artifacts_directory(&self) -> &ArtifactsLayout {
        self.frontmatter().artifacts_directory()
    }

    /// Get the manifest constructed from the script's frontmatter.
    pub fn manifest(&self) -> &Manifest {
        self.frontmatter().manifest()
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

    /// Convert this script to an [`Identity`].
    /// This will always (try to) return an [`Identity`] with [`Origin::for_local`] origin.
    pub fn as_a_local_identity(&self) -> QuackResult<Identity> {
        let origin = Origin::for_local(self.frontmatter().script_file())?;
        Ok(Identity::new(self.manifest().name(), origin))
    }

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        self.into_frontmatter().into_manifest()
    }

    /// Transform into the underlying frontmatter.
    pub fn into_frontmatter(self) -> FrontMatter {
        self.frontmatter
    }
}

#[derive(Clone)]
/// A frontmatter of a [`StandaloneScript`].
pub struct FrontMatter {
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

impl FrontMatter {
    /// Create a new [`FrontMatter`].
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

    /// Transform into the underlying manifest.
    pub fn into_manifest(self) -> Manifest {
        self.manifest
    }
}

impl fmt::Debug for FrontMatter {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FrontMatter")
            .field("manifest", &self.manifest)
            .field("script path", &self.path)
            .finish_non_exhaustive()
    }
}
