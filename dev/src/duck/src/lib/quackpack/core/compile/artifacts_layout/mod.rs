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

    fn new(root: PathBuf) -> Self;
    fn root_directory(&self) -> &Path;
    fn acquire_global_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile>;
    fn for_profile(&self, profile: Profile) -> impl ProfileLayout;
    fn file_lock_manager(&self) -> &FileLockManager;
}

pub trait ProfileLayout {
    fn file_lock_manager(&self) -> &FileLockManager;
    fn root_directory(&self) -> &Path;
    fn for_dependency(&self, unit: &Unit, _graph: &UnitGraph)
    -> QuackResult<impl DependencyLayout>;
}

pub trait DependencyLayout {
    const LOCK_NAME: &str = ".duck_lock";
    const JSON_NAME: &str = "deps.json";

    fn file_lock_manager(&self) -> &FileLockManager;
    fn root_directory(&self) -> &Path;
    fn acquire_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile>;
    fn compiler_artifacts(&self) -> PathBuf;
    fn dependency_json(&self, ctx: &DuckContext) -> QuackResult<LockedFile>;
    fn dependency_json_path(&self) -> PathBuf;
}
