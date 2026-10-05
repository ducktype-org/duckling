// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

use std::path::PathBuf;

use super::{ArtifactsLayout, DependencyLayout, ProfileLayout};
use crate::QuackResult;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::util::file_locks::FileLockManager;

#[derive(Clone, Debug)]
/// Standard layout of the artifacts directory. See the [module](super::standard_artifacts_layout) documentation.
pub struct StandardArtifactsLayout {
    root: FileLockManager,
}

impl StandardArtifactsLayout {
    /// Create a new [`StandardArtifactsLayout`].
    pub fn new(root: PathBuf) -> Self {
        Self {
            root: FileLockManager::new(root),
        }
    }
}

impl ArtifactsLayout for StandardArtifactsLayout {
    fn for_profile(&self, profile: Profile) -> Box<dyn ProfileLayout> {
        Box::new(StandardProfileLayout {
            root: self.root.join(profile.name),
        })
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

    fn for_dependency(
        &self,
        unit: &Unit,
        _graph: &UnitGraph,
    ) -> QuackResult<Box<dyn DependencyLayout>> {
        Ok(Box::new(StandardDependencyLayout {
            root: self.root.join(unit.unique_name()),
        }))
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
}
