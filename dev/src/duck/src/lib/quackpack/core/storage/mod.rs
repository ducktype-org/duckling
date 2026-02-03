//! Provides high level storage operations. Access control is provided by [`locks`]
//! module. Operations for state modification, which preserve coherency, are
//! provided by [`files`] module.

mod files;
mod git_access;
mod locks;
mod ops;
mod package_id;
mod paths;
pub use ops::*;
