//! Layout of the QuackPack's artefacts directory.
//!
//! <artefacts root>
//! ├── <profile name>/
//! │   ├── <dependency artefacts>/
//! │   │   ├── <generated artefacts of the dependency>
//! │   │   ├── artefacts/ # Duckc artefacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock # Global artefacts lock
//! │   └── <useful artefacts of the root package> # artefacts like main executable, main binary, etc
//! └── .duck_lock # Global artefacts lock

use std::path::{Path, PathBuf};

use crate::{
    DuckContext, QuackResult,
    util::file_locks::{FileLockManager, LockedFile},
};

#[derive(Clone, Debug)]
/// Layout of the artefacts directory. See the [module](super::artefacts_layout) documentation.
pub struct ArtefactsLayout {
    root: FileLockManager,
}

impl ArtefactsLayout {
    const GLOBAL_LOCK_NAME: &str = ".duck_lock";

    /// Create a new [`ArtefactsLayout`].
    pub fn new(root: PathBuf) -> Self {
        Self {
            root: FileLockManager::new(root),
        }
    }

    /// Get a root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire a global artefacts lock.
    pub fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::GLOBAL_LOCK_NAME, ctx)
    }

    /// Get a layout for a specific profile name.
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

    /// Get a root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Get a layout for a specific dependency.
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

    /// Get a root directory [`Path`] for this layout.
    pub fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Acquire a lock for this dependency's artefacts.
    pub fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::LOCK_NAME, ctx)
    }

    /// Get the path for compiler artefacts.
    pub fn compiler_artefacts(&self) -> PathBuf {
        self.root_directory().join("artefacts")
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
