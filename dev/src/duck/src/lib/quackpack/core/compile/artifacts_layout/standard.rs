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
/// Standard layout of the artifacts directory. See the [module](super::standard_artifacts_layout) documentation.
pub struct StandardArtifactsLayout {
    root: FileLockManager,
}

impl ArtifactsLayout for StandardArtifactsLayout {
    fn new(root: PathBuf) -> Self {
        Self {
            root: FileLockManager::new(root),
        }
    }

    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::GLOBAL_LOCK_NAME, ctx)
    }

    fn for_profile(&self, profile: Profile) -> impl ProfileLayout {
        StandardProfileLayout {
            root: self.root.join(profile.name),
        }
    }

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
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

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
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::LOCK_NAME, ctx)
    }

    fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root.open_exclusive(Self::JSON_NAME, ctx)
    }

    fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(Self::JSON_NAME)
    }
}
