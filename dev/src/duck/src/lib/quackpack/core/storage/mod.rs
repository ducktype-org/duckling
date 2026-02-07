//! Provides high level storage operations. Access control is provided by [`locks`]
//! module. Operations for state modification, which preserve coherency, are
//! provided by [`files`] module.

pub mod freeze;
pub mod git_access;
pub mod locks;
pub mod ops;
pub mod package_id;
pub mod paths;
pub mod venv;
pub use ops::*;
