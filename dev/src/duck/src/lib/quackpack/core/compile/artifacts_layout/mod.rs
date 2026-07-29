use std::path::{Path, PathBuf};

use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

pub mod shared;
pub mod standard;

pub trait ArtifactsLayout {
    const GLOBAL_LOCK_NAME: &str = ".duck_lock";

    /// Create new [`ArtifactsLayout`].
    fn new(root: PathBuf) -> Self;
    /// Get the layout for a specific profile.
    fn for_profile(&self, profile: Profile) -> impl ProfileLayout;
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get a root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }

    /// Acquire the global artifacts lock.
    fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(Self::GLOBAL_LOCK_NAME, ctx)
    }
}

pub trait ProfileLayout {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;
    /// Get the layout for a specific dependency.
    fn for_dependency(&self, unit: &Unit, graph: &UnitGraph) -> QuackResult<impl DependencyLayout>;

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }
}

pub trait DependencyLayout {
    const LOCK_NAME: &str = ".duck_lock";
    const JSON_NAME: &str = "deps.json";

    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }

    /// Acquire a lock for this dependency's artifacts.
    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(Self::LOCK_NAME, ctx)
    }

    /// Get the path for compiler artifacts.
    fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    /// Get the [`PathBuf`] where dependencies JSON should be stored.
    fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(Self::JSON_NAME)
    }

    /// Acquire a lock for the JSON of dependencies of this dependency.
    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(Self::JSON_NAME, ctx)
    }
}
