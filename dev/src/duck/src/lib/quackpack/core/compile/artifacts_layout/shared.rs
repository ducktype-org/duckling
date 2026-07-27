//! Layout of the QuackPack's artifacts directory.
//!
//! For main package:
//! <artifacts root>
//! ├── <profile name>/
//! │   ├── *root package artifacts*/
//! │   │   ├── artifacts/ # Duckc artifacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock
//! │   └── *useful artifacts of the root package* # artifacts like main executable, main binary, compiled scripts, etc
//! └── .duck_lock # Global artifacts lock
//!
//! For local dependencies:
//! <dependency artifacts root>
//! └── <hash of unit subgraph and profile>/
//!     ├── *generated artifacts of the dependency*
//!     ├── artifacts/ # Duckc artifacts directory
//!     ├── deps.json # JSON used to communicate between QuackPack and duckc.
//!     └── .duck_lock # Per dependency lock
//!
//! For local dependencies:
//! <storage root>/pkg/.duck_build/
//! └── <hash of unit subgraph and profile>/
//!     ├── *generated artifacts of the dependency*
//!     ├── artifacts/ # Duckc artifacts directory
//!     ├── deps.json # JSON used to communicate between QuackPack and duckc.
//!     └── .duck_lock # Per dependency lock

use std::path::{Path, PathBuf};

use itertools::Itertools;

use crate::quackpack::core::compile::artifacts_layout::{
    ArtifactsLayout, DependencyLayout, ProfileLayout,
};
use crate::quackpack::core::compile::executor::collect_packages;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::util::hash::sha256_string;
use crate::{DuckContext, QuackResult};

fn hash_subgraph_and_profile(
    unit: &Unit,
    graph: &UnitGraph,
    profile: Profile,
) -> QuackResult<String> {
    let subgraph_string = collect_packages(unit, graph)?
        .iter()
        .map(|pkg| pkg.id)
        .join(",");
    let to_hash = format!("{subgraph_string}-{}", profile.serialize_raw());
    Ok(sha256_string(to_hash))
}

#[derive(Debug, Clone)]
pub struct SharedArtifactsLayout {
    root: FileLockManager,
}

impl ArtifactsLayout for SharedArtifactsLayout {
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
        SharedProfileLayout {
            root: self.root.join(profile.name),
            profile,
        }
    }

    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }
}

#[derive(Debug, Clone)]
pub struct SharedProfileLayout {
    root: FileLockManager,
    profile: Profile,
}

impl ProfileLayout for SharedProfileLayout {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager {
        &self.root
    }

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.root.not_locked_path()
    }

    /// Get the layout for a specific dependency.
    fn for_dependency(&self, unit: &Unit, graph: &UnitGraph) -> QuackResult<impl DependencyLayout> {
        let path = if graph.is_root(unit) {
            self.root_directory().join(unit.unique_name())
        } else {
            let hash = hash_subgraph_and_profile(unit, graph, self.profile)?;
            unit.root_package()
                .package()
                .artifacts_directory()
                .join(hash)
        };
        Ok(SharedDependencyLayout {
            root: FileLockManager::new(path),
        })
    }
}

#[derive(Debug, Clone)]
pub struct SharedDependencyLayout {
    root: FileLockManager,
}

impl DependencyLayout for SharedDependencyLayout {
    const LOCK_NAME: &str = ".duck_lock";
    const JSON_NAME: &str = "deps.json";

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
