//! Layout of the QuackPack's artifacts directory.
//!
//! <artifacts root>
//! ├── <profile name>/
//! │   ├── *dependency artifacts*/
//! │   │   ├── *generated artifacts of the dependency*
//! │   │   ├── artifacts/ # Duckc artifacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock # Per dependency lock
//! │   └── *useful artifacts of the root package* # artifacts like main executable, main binary, etc
//! └── .duck_lock # Global artifacts lock

use std::path::{Path, PathBuf};

use crate::{
    DuckContext, QuackResult,
    util::file_locks::{FileLockManager, LockedFile},
};

#[derive(Clone, Debug)]
/// Layout of the artifacts directory. See the [module](super::artifacts_layout) documentation.
pub struct ArtifactsLayout {
    root: FileLockManager,
}

impl ArtifactsLayout {
    const GLOBAL_LOCK_NAME: &str = ".duck_lock";

    /// Create a new [`ArtifactsLayout`].
    pub fn new(root: PathBuf) -> Self {
        Self {
            root: FileLockManager::new(root),
        }
    }

    /// Get a root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire the global artifacts lock.
    pub fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::GLOBAL_LOCK_NAME, ctx)
    }

    /// Get the layout for a specific profile name.
    pub fn for_profile(&self, name: &str) -> ProfileLayout {
        ProfileLayout {
            root: self.root.join(name),
        }
    }

    /// Get the [`FileLockManager`] for this layout.
    pub fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }
}

#[derive(Debug, Clone)]
/// A struct responsible for a layout of a specific profile.
pub struct ProfileLayout {
    root: FileLockManager,
}

impl ProfileLayout {
    /// Get the [`FileLockManager`] for this layout.
    pub fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    /// Get the root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Get the layout for a specific dependency.
    pub fn for_dependency(&self, name: &str) -> DependencyLayout {
        DependencyLayout {
            root: self.root.join(name),
        }
    }
}

#[derive(Debug, Clone)]
/// A struct responsible for a layout of a specific dependency.
pub struct DependencyLayout {
    root: FileLockManager,
}

impl DependencyLayout {
    const LOCK_NAME: &str = ".duck_lock";
    const JSON_NAME: &str = "deps.json";

    /// Get the [`FileLockManager`] for this layout.
    pub fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    /// Get the root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire a lock for this dependency's artifacts.
    pub fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::LOCK_NAME, ctx)
    }

    /// Get the path for compiler artifacts.
    pub fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    /// Acquire a lock for the JSON of dependencies of this dependency.
    pub fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::JSON_NAME, ctx)
    }

    /// Get the [`PathBuf`] where dependencies JSON should be stored.
    pub fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(Self::JSON_NAME)
    }
}
