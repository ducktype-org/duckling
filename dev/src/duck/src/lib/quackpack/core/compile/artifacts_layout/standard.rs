//! Layout of the QuackPack's artifacts directory.
//!
//! <artifacts root>
//! ├── <profile name>/
//! │   ├── *dependency artifacts*/
//! │   │   ├── *generated artifacts of the dependency*
//! │   │   ├── artifacts/ # Duckc artifacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock # Per dependency lock
//! │   ├── *root package artifacts*/
//! │   │   ├── artifacts/ # Duckc artifacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock
//! │   └── *useful artifacts of the root package* # artifacts like main executable, main binary, compiled scripts, etc
//! └── .duck_lock # Global artifacts lock

use std::path::{Path, PathBuf};

use crate::quackpack::core::compile::artifacts_layout::{
    ArtifactsLayout, DependencyLayout, ProfileLayout,
};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

#[derive(Clone, Debug)]
/// Layout of the artifacts directory. See the [module](super::artifacts_layout) documentation.
pub struct StandardArtifactsLayout {
    root: FileLockManager,
}

impl ArtifactsLayout for StandardArtifactsLayout {
    const GLOBAL_LOCK_NAME: &str = ".duck_lock";

    /// Create a new [`ArtifactsLayout`].
    fn new(root: PathBuf) -> Self {
        Self {
            root: FileLockManager::new(root),
        }
    }

    /// Get a root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire the global artifacts lock.
    fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::GLOBAL_LOCK_NAME, ctx)
    }

    /// Get the layout for a specific profile name.
    fn for_profile(&self, profile: Profile) -> impl ProfileLayout {
        StandardProfileLayout {
            root: self.root.join(profile.name),
        }
    }

    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }
}

#[derive(Debug, Clone)]
/// A struct responsible for a layout of a specific profile.
pub struct StandardProfileLayout {
    root: FileLockManager,
}

impl ProfileLayout for StandardProfileLayout {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Get the layout for a specific dependency.
    fn for_dependency(
        &self,
        unit: &Unit,
        _graph: &UnitGraph,
    ) -> QuackResult<impl DependencyLayout> {
        Ok(StandardDependencyLayout {
            root: self.root.join(unit.unique_name()),
        })
    }
}

#[derive(Debug, Clone)]
/// A struct responsible for a layout of a specific dependency.
pub struct StandardDependencyLayout {
    root: FileLockManager,
}

impl DependencyLayout for StandardDependencyLayout {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire a lock for this dependency's artifacts.
    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::LOCK_NAME, ctx)
    }

    /// Get the path for compiler artifacts.
    fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    /// Acquire a lock for the JSON of dependencies of this dependency.
    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::JSON_NAME, ctx)
    }

    /// Get the [`PathBuf`] where dependencies JSON should be stored.
    fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(Self::JSON_NAME)
    }
}
