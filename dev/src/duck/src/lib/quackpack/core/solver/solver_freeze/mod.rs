mod freeze_diagnosis;
mod new_freeze_generation;

use std::collections::{HashMap, HashSet};

use crate::{
    StrId,
    quackpack::core::{FeatureName, types_common::ExpandedPackage},
};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverFreeze {
    pub package_freezes: HashMap<ExpandedPackage, SolverPackageFreeze>,
    pub main_pkg: ExpandedPackage,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverPackageFreeze {
    pub dependencies_realization: HashMap<StrId, ExpandedPackage>,
    pub features: HashSet<FeatureName>,
}

impl SolverPackageFreeze {
    pub fn new() -> Self {
        Self {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        }
    }
}

impl Default for SolverPackageFreeze {
    fn default() -> Self {
        Self::new()
    }
}
