use crate::quackpack::core::solver::types::{ExpandedLocation, ExpandedPackage};

#[derive(Debug, Eq, Hash, PartialEq)]
pub struct ParentWithDependencyLoc {
    pub parent: ExpandedPackage,
    pub dependency_loc: ExpandedLocation,
}
