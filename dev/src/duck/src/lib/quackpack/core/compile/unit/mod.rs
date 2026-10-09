// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! [`Unit`] is a single task to be performed during the compilation pipeline.
//! This is not to be misleaded with [`Task`](crate::quackpack::core::compile::duckc::multipackage_schema::Task),
//! which represents a low-level duckc task.

use std::collections::HashSet;
use std::env::consts::{DLL_PREFIX, DLL_SUFFIX, EXE_SUFFIX};
use std::hash::Hash;
use std::sync::Arc;

use super::duckc::multipackage_schema;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit_runner::CompilationTarget;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::{AnyPackage, FeatureName};
use crate::util::hash::sha256_string;
use crate::{QuackResult, StrId, qp_bail_internal};

pub mod graph;
pub mod graph_visitor;

#[cfg(test)]
mod tests;

// Missing constants from [`std::env::consts`].
const STATIC_LIB_SUFFIX: &str = ".a";

// Duckling specific.
const DVM_SUFFIX: &str = ".dbc";

#[derive(Clone, Debug)]
/// A single action to be performed during the compilation process.
pub struct Unit {
    /// Necessary information about the package for which this [`Unit`] is constructed.
    package_data: Arc<PackageData>,
    /// Type of the task to perform.
    unit_type: UnitType,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// What type of artifacts a given [`Unit`] produces.
pub enum UnitType {
    /// Compile to a binary
    /// Maps to the `Native` strategy
    Binary,
    /// Compile to a library (`.dll`, `.so`, `.a`, etc)
    /// Maps to the `Native` strategy
    Library,
    /// This [`Unit`] is a dependency and can produce only minimal artifacts
    /// Maps to the `Lib` compilation strategy, and we'll produce only minimal archives:
    /// they might be incomplete, but linker will take care of this (when compiling the root package
    /// with [`Binary`](Self::Binary) or [`Library`](Self::Library) types).
    Dependency,
}

impl Unit {
    /// Create a new [`Unit`].
    pub fn new(package: Arc<PackageData>, unit_type: UnitType) -> Self {
        Self {
            package_data: package,
            unit_type,
        }
    }

    /// Get the package of this [`Unit`].
    pub fn package(&self) -> &AnyPackage {
        &self.package_data.package
    }

    /// Get the [`PackageData`] of the package of this [`Unit`].
    pub fn package_data(&self) -> &PackageData {
        &self.package_data
    }

    /// Get the enabled features of this [`Unit`].
    pub fn enabled_features(&self) -> &HashSet<FeatureName> {
        &self.package_data.enabled_features
    }

    /// Get the type of produced artifacts by this [`Unit`].
    pub fn unit_type(&self) -> UnitType {
        self.unit_type
    }

    /// Get the [`Identity`] of this [`Unit`].
    pub fn identity(&self) -> Identity {
        self.package_data.identity
    }

    /// Get a unique (in terms of the current compilation graph) name of the underlying package.
    /// It can be used as a directory name for storing artifacts.
    pub fn pkg_unique_name(&self) -> String {
        self.package_data().unique_name()
    }

    /// Get a descriptive name of the underlying package.
    ///
    /// It's a _nice_ name, which can be displayed to the user.
    pub fn pkg_descriptive_name(&self) -> String {
        let name = self.package().name();
        let version = self.package().version();
        format!("{name} version {version}")
    }

    /// Get the filename of the output of this [`Unit`].
    pub fn output_file_name(&self, bcx: &BuildContext<'_, '_>) -> String {
        let name = self.package().name();
        match (self.unit_type(), bcx.compilation_target()) {
            // LLVM
            (UnitType::Binary, CompilationTarget::LLVM) => format!("{}{}", name, EXE_SUFFIX),
            (UnitType::Library, CompilationTarget::LLVM) => {
                format!("{}{}{}", DLL_PREFIX, name, DLL_SUFFIX)
            }
            (UnitType::Dependency, CompilationTarget::LLVM) => {
                format!("{}{}", self.pkg_unique_name(), STATIC_LIB_SUFFIX)
            }

            // DVM
            (UnitType::Binary | UnitType::Library, CompilationTarget::DVM) => {
                format!("{}{}", name, DVM_SUFFIX)
            }
            (UnitType::Dependency, CompilationTarget::DVM) => {
                format!("{}{}", self.pkg_unique_name(), DVM_SUFFIX)
            }
        }
    }
}

/// All compilation-necessary information about a singular package.
#[derive(Clone, Debug)]
pub struct PackageData {
    package: AnyPackage,
    /// Pairs `(pkg, alias)`.
    deps_realization: Vec<(Identity, Option<StrId>)>,
    enabled_features: HashSet<FeatureName>,
    identity: Identity,
}

impl PackageData {
    /// Get a unique (in terms of the current compilation graph) name of the package.
    /// It can be used as a directory name for storing artifacts.
    pub fn unique_name(&self) -> String {
        let id = sha256_string(self.identity.origin().to_string());
        let name = self.package.name();
        let version = self.package.version();
        format!("{}-{}-{}", name, version, id)
    }

    /// Get a single [`multipackage_schema::Package`] for this package.
    pub fn multipackage_schema_package(
        &self,
        graph: &UnitGraph,
    ) -> QuackResult<multipackage_schema::Package> {
        let package = &self.package;
        let import_name = package.normalised_name();
        let version = package.version();
        let features = {
            let mut features = self.enabled_features.iter().copied().collect::<Vec<_>>();
            features.sort();
            features
        };
        let dependencies = self
            .deps_realization
            .iter()
            .map(|(dep, alias)| multipackage_schema::Dependency {
                id: graph.package_data(*dep).unique_name().into(),
                alias: alias.map(|alias| alias.to_string()),
            })
            .collect();
        let Some(source_directory) = package.src() else {
            qp_bail_internal!(
                "asked for src directory of the global package or a script: {package:#?}"
            )
        };
        Ok(multipackage_schema::Package {
            id: self.unique_name().into(),
            import_name,
            version,
            features,
            path_to_the_src_directory: source_directory.to_path_buf(),
            dependencies,
        })
    }
}

impl PartialEq for Unit {
    fn eq(&self, other: &Self) -> bool {
        self.unit_type == other.unit_type && Arc::ptr_eq(&self.package_data, &other.package_data)
    }
}

impl Eq for Unit {}

impl Hash for Unit {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        let ptr = Arc::as_ptr(&self.package_data);
        std::ptr::hash(ptr, state);
        self.unit_type.hash(state);
    }
}
