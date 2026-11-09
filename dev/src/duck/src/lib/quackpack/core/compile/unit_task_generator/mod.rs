// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! [`UnitTaskGenerator`] describes which [`Unit`]s to compile and creates appropriate [`Task`]s for them.
//!
//! [`Task`]: multipackage_schema::PackageCompilationTask

use std::fmt::Debug;

pub mod default;
pub mod dvm;

use super::BuildContext;
use super::artifacts_layout::ProfileLayout;
use super::duckc::multipackage_schema;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::QuackResult;
use crate::quackpack::core::compile::unit::graph::GraphNodeId;

/// A generic duckc driver.
pub trait UnitTaskGenerator: Debug {
    /// Callback invoked at the very start of [`run`].
    ///
    /// Right now, this function performs [`assert`]sions about the mode (f.e. that
    /// [`DvmTaskGenerator`] actually tries to compile DVM packages).
    ///
    /// It's allowed that this function will return an [`Err`], which is not a logic error, but a
    /// normal _user_ error (f.e. linking against external libraries on DVM).
    ///
    /// [`run`]: super::UnitRunner::run
    /// [`DvmTaskGenerator`]: dvm::DvmTaskGenerator
    fn pre_compilation(&self, graph: &UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<()>;

    /// Create tasks which will be used for compiling the given [`Unit`].
    fn create_tasks(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>>;

    /// Whether the given [`Unit`] should be compiled.
    ///
    /// Note that, in the future, some [`Unit`]s (mainly build scripts) will _always_ be compiled,
    /// no matter what this function returns.
    fn should_run(&self, unit: &Unit, graph: &UnitGraph, bcx: &BuildContext<'_, '_>) -> bool;
}
