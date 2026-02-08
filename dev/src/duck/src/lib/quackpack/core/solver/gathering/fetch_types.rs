use std::{collections::HashSet, path::PathBuf};

use crate::quackpack::core::{
    FeatureName, Version,
    types_common::{InternedLocation, Package},
};

pub enum FetchRequest {
    Pinned(PinnedRequest),
    Unpinned(UnpinnedRequest),
}

pub struct UnpinnedRequest {
    pub location: InternedLocation,
    pub versions: Vec<Version>,
    pub features: HashSet<FeatureName>,
    pub local_root: Option<PathBuf>,
}

pub struct PinnedRequest {
    pub package: Package,
    pub features: HashSet<FeatureName>,
    pub local_root: Option<PathBuf>,
}
