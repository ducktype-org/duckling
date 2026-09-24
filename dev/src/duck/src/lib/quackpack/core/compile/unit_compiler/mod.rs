//! [`UnitCompiler`] describes which [`Unit`]s to compile and creates appropriate [`Task`]s for them.
//!
//! [`Task`]: multipackage_schema::PackageCompilationTask

use std::fmt::Debug;

pub mod default_unit_compiler;
pub mod dvm_unit_compiler;

use super::BuildContext;
use super::artifacts_layout::ProfileLayout;
use super::duckc::multipackage_schema;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::QuackResult;

/// A generic duckc driver.
pub trait UnitCompiler: Debug {
    /// Callback invoked at the very start of [`compile`].
    ///
    /// Right now, this function performs [`assert`]sions about the mode (f.e. that
    /// [`DvmUnitCompiler`] actually tries to compile DVM packages).
    ///
    /// It's allowed that this function will return an [`Err`], which is not a logic error, but a
    /// normal _user_ error (f.e. linking against external libraries on DVM).
    fn pre_compilation(&self, graph: &UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<()>;

    /// Create tasks which will be used for compiling the given [`Unit`].
    fn create_tasks(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>>;

    /// Whether the given [`Unit`] should be compiled.
    ///
    /// Note that, in the future, some [`Unit`]s (mainly build scripts) will _always_ be compiled,
    /// no matter what this function returns.
    fn should_compile(&self, unit: &Unit, graph: &UnitGraph, bcx: &BuildContext<'_, '_>) -> bool;
}
