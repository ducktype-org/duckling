use std::collections::HashMap;

use crate::quackpack::core::{Dependency, Source};
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext, StrId};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
/// A struct describing a dependency of a package on some location.
pub struct DependencyEdge {
    pub parent: WithVersion<FullIdentity>,
    pub dep_identity: FullIdentity,
    pub manifest_child_name: StrId,
}

impl DependencyEdge {
    /// Given a package and a manifest entry describing its dependency,
    /// creates a [`DependencyEdge`].
    pub fn from_manifest_and_parent(
        parent: WithVersion<FullIdentity>,
        manifest_dependency: &Dependency,
        location_resolver: &HashMap<Source, FullOrigin>,
    ) -> QuackResult<Self> {
        location_resolver
            .get(manifest_dependency.source())
            .map(|child_origin| Self {
                parent,
                dep_identity: FullIdentity::new(manifest_dependency.name(), *child_origin),
                manifest_child_name: manifest_dependency.effective_name(),
            })
            .context_internal("Failed to expand a location")
    }
}
