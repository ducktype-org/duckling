// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::{Path, PathBuf};

use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

pub mod shared;
pub mod standard;

pub trait ArtifactsLayout {
    fn global_lock_name(&self) -> &'static str {
        ".duck_lock"
    }

    /// Get the layout for a specific profile.
    fn for_profile(&self, profile: Profile) -> Box<dyn ProfileLayout>;

    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get a root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }

    /// Acquire the global artifacts lock.
    fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(self.global_lock_name(), ctx)
    }
}

pub trait ProfileLayout {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get the layout for a specific dependency.
    fn for_dependency(
        &self,
        unit_id_in_graph: GraphNodeId,
        graph: &UnitGraph,
    ) -> QuackResult<Box<dyn DependencyLayout>>;

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }
}

pub trait DependencyLayout {
    fn lock_name(&self) -> &'static str {
        ".duck_lock"
    }

    fn json_name(&self) -> &'static str {
        "deps.json"
    }

    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }

    /// Acquire a lock for this dependency's artifacts.
    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(self.lock_name(), ctx)
    }

    /// Get the path for compiler artifacts.
    fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    /// Get the [`PathBuf`] where dependencies JSON should be stored.
    fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(self.json_name())
    }

    /// Acquire a lock for the JSON of dependencies of this dependency.
    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(self.json_name(), ctx)
    }
}
