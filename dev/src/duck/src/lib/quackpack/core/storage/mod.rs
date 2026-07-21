//! Provides high level storage operations. Access control is provided by [`locks`]
//! module. Operations for state modification, which preserve coherency, are
//! provided by [`venv`] module.
//!
//! Very good reading entry point are [`ops::sync`], [`ops::clean_storage`], and
//! [`ops::delete_venv`].
//!
//! Modules outside of [`ops`] serve as a general API/support for [`ops`].

pub mod freeze;
pub mod git_access;
pub mod locks;
pub mod ops;
pub mod package_id;
pub mod paths;
pub mod venv;
use std::fs::DirEntry;

pub use ops::*;
pub mod venv_id;

/// Type of an iterator over contents of a directory.
pub type DirContentsIterator = Box<dyn Iterator<Item = Result<DirEntry, std::io::Error>>>;

#[cfg(test)]
mod tests;
