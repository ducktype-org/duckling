mod dependency_edge;
mod expanded;

pub use dependency_edge::ParentWithDependencyLoc;
pub use expanded::{ExpandedLocation, ExpandedPackage};

use crate::quackpack::core::FeatureName;

pub type PresentFeature = Option<FeatureName>;
