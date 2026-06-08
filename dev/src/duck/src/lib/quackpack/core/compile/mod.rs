//! Main entrypoint for compiling an entire project.
//!
//! Notable modules are:
//! - [`early_graph`][]: creating and modifying dependency graphs; notably, it checks for cycles,
//!   expands features, and removes disabled dependencies,
//! - [`duckc`][]: executing the compiler itself, it handles different compiler execution modes.
use std::path::PathBuf;

use tracing::debug;

use crate::QuackResult;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::{FeatureName, PackageContext};

pub mod artifacts_layout;
pub mod compiler_package;
pub mod duckc;
pub mod early_graph;
pub mod profiles;
pub mod unit;

use duckc::*;

use self::early_graph::creating_graph::create_early_graph_from_bcx;

/// A common message for panicking when a manifest is missing a dependency.
pub fn missing_depenendcy_in_manifest_message(root_name: &str, dep: &str) -> String {
    format!("malformed manifest of `{root_name}`: missing dependency `{dep}` in the manifest")
}

/// A common message for panicking when any graph is missing a key.
pub fn missing_depenendcy_in_graph_message(id: Identity) -> String {
    format!("missing dependency `{id}` in the graph")
}

#[derive(Debug)]
/// All informations required to compile a project.
pub struct BuildContext<'duck, 'ctx> {
    /// Package to build or venv of the script.
    pub pcx: &'ctx PackageContext<'duck>,
    pub root_identity: Identity,
    pub freeze: VenvFreeze,
    pub storage: Storage,
    pub used_features: Vec<FeatureName>,
    pub profile: Profile,
    pub script_path: Option<PathBuf>,
}

/// Compile project inside the [`BuildContext`].
#[tracing::instrument(skip_all)]
pub fn compile(
    bcx: BuildContext<'_, '_>,
    compilation_type: CompilationType,
) -> QuackResult<ArtifactsDir> {
    debug!(bcx = ?bcx, "compiling");
    let graph = create_early_graph_from_bcx(&bcx)?;
    let duckc = Duckc::new(bcx.pcx.ctx());
    duckc.compile(&graph, compilation_type, &bcx)
}
