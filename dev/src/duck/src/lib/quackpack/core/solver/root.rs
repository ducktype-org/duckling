// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::HashSet;
use std::path::PathBuf;

use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::{FeatureName, Manifest, PackageContext, PackageId};
use crate::{QuackResult, qp_bail_internal};

/// The root package of the solving process, i.e. the local package whose dependencies are resolved.
///
/// The root is always solved with all the features listed in its manifest.
#[derive(Debug)]
pub(super) struct SolverRoot {
    pub id: PackageId,
    pub path: PathBuf,
    pub manifest: Box<Manifest>,
    pub features: HashSet<FeatureName>,
}

impl SolverRoot {
    /// Describes the local package of the given [`PackageContext`] as the solving root.
    pub fn new(pcx: &PackageContext<'_>) -> QuackResult<Self> {
        let manifest = Box::new(pcx.package().manifest().clone());
        let features = manifest.features().all_features().keys().copied().collect();
        Ok(Self {
            id: pcx.local_package_id()?,
            path: pcx.package().root().into(),
            manifest,
            features,
        })
    }

    /// Returns the root manifest restricted to the dependencies which are not realized in `freeze`,
    /// so that the gathering process only explores what still has to be resolved.
    pub fn manifest_for_gathering(&self, freeze: &SolverFreeze) -> QuackResult<Box<Manifest>> {
        let Some(root_freeze) = freeze.package_freezes.get(&self.id) else {
            qp_bail_internal!("maximal valid freeze without main package freeze {freeze:#?}")
        };
        let mut manifest = self.manifest.clone();
        manifest
            .dependencies_mut()
            .all_dependencies_mut()
            .retain(|dep| {
                !root_freeze
                    .dependencies_realization
                    .contains_key(&dep.effective_name())
            });
        Ok(manifest)
    }
}
