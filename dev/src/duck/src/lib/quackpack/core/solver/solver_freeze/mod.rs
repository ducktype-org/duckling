mod freeze_diagnosis;
mod new_freeze_generation;

use std::collections::{HashMap, HashSet};

use crate::{
    StrId,
    quackpack::core::{FeatureName, types_common::ExpandedPackage},
};

#[derive(Clone, Debug)]
pub struct VenvFreeze {
    pub package_freezes: HashMap<ExpandedPackage, PackageFreeze>,
    pub main_pkg: ExpandedPackage,
}

#[derive(Clone, Debug)]
pub struct PackageFreeze {
    pub dependencies_realization: HashMap<StrId, ExpandedPackage>,
    pub features: HashSet<FeatureName>,
}

impl PackageFreeze {
    pub fn new() -> Self {
        Self {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        }
    }
}

impl Default for PackageFreeze {
    fn default() -> Self {
        Self::new()
    }
}
