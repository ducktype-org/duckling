use std::collections::HashMap;

use crate::StrId;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::{Dependency, PackageId, Source};

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
/// A struct describing a dependency of a package on some identity.
pub struct DependencyEdge {
    pub parent: PackageId,
    pub dep_identity: FullIdentity,
    pub manifest_child_name: StrId,
}

impl DependencyEdge {
    /// Given a package and a manifest entry describing its dependency,
    /// creates a [`DependencyEdge`].
    pub fn from_manifest_and_parent(
        parent: PackageId,
        manifest_dependency: &Dependency,
        source_to_origin_resolver: &HashMap<Source, FullOrigin>,
    ) -> Option<Self> {
        source_to_origin_resolver
            .get(&manifest_dependency.source())
            .map(|child_origin| Self {
                parent,
                dep_identity: FullIdentity::new(manifest_dependency.name(), *child_origin),
                manifest_child_name: manifest_dependency.effective_name(),
            })
    }
}
