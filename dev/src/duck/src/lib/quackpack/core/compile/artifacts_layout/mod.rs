// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::fmt;
use std::path::{Path, PathBuf};

use super::unit_runner::incremental::FingerprintManager;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

pub mod shared;
pub mod standard;

const GLOBAL_LOCK_NAME: &str = ".duck_lock";
const FINGERPRINT_DIR_NAME: &str = ".fingerprint";
const DEPENDENCY_LOCK_NAME: &str = ".duck_lock";
const DEPENDENCY_JSON_NAME: &str = "deps.json";

pub trait ArtifactsLayout: fmt::Debug {
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
            .open_exclusive(GLOBAL_LOCK_NAME, ctx)
    }

    /// Name of the [`ArtifactsLayout`] implementation.
    /// Should be unique from other implementations.
    fn name(&self) -> &'static str;
}

pub trait ProfileLayout: fmt::Debug {
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

    /// Get the [`LockedFile`] for the fingerprint.
    fn fingerprint(&self) -> FingerprintManager {
        let manager = self.file_lock_manager().join(FINGERPRINT_DIR_NAME);
        FingerprintManager::new(manager)
    }
}

pub trait DependencyLayout: fmt::Debug {
    /// Get the [`FileLockManager`] for this layout.
    fn file_lock_manager(&self) -> &FileLockManager;

    /// Get the root directory [`Path`] for this layout.
    fn root_directory(&self) -> &Path {
        self.file_lock_manager().not_locked_path()
    }

    /// Acquire a lock for this dependency's artifacts.
    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(DEPENDENCY_LOCK_NAME, ctx)
    }

    /// Get the path for compiler artifacts.
    fn compiler_artifacts(&self) -> PathBuf {
        self.root_directory().join("artifacts")
    }

    /// Get the [`PathBuf`] where dependencies JSON should be stored.
    fn dependency_json_path(&self) -> PathBuf {
        self.root_directory().join(DEPENDENCY_JSON_NAME)
    }

    /// Acquire a lock for the JSON of dependencies of this dependency.
    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.file_lock_manager()
            .open_exclusive(DEPENDENCY_JSON_NAME, ctx)
    }
}
