//! A module containing different ways of identifying packages.
//! All the intricacies are described in the `readme.md`.

pub mod full_identity;
pub mod identity;
mod package_id;

pub use package_id::*;
