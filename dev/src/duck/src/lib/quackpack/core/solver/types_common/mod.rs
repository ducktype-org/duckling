mod dependency_edge;
mod expanded;
mod not_expanded;

pub use dependency_edge::DependencyEdge;
pub use expanded::{ExpandedLocation, ExpandedLocGit, ExpandedLocLocal, ExpandedLocRegistry, ExpandedPackage};
pub use not_expanded::{Location, LocGit, LocLocal, LocRegistry, Package};

use crate::quackpack::core::FeatureName;

pub type PresentFeature = Option<FeatureName>;
