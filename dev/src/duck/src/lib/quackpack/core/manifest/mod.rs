//! High-level abstraction on manifest and its inner types.
//!
//! The most notable members are [`Manifest`], [`Summary`], [`Dependency`],
//! [`DependencyDescription`] and [`RootDescription`].
//!
//! Parsing will be implemented in a `parse` module, in future PRs.
mod compiler_options;
mod dependency;
mod features;
// NOTE: This module has a different name, because rust emits lints, when there's file in a module with the same name.
mod manifest_struct;
mod root_description;
mod source;
mod summary;

pub use compiler_options::*;
pub use dependency::*;
pub use features::*;
pub use manifest_struct::*;
pub use root_description::*;
pub use source::*;
pub use summary::*;
