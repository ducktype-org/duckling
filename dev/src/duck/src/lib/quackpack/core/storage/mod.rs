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
pub mod paths;
pub mod venv;
use std::fs::{DirEntry, ReadDir};

pub use ops::*;
pub mod venv_id;

/// Type of an iterator over contents of a directory.
pub enum DirContents {
    Empty,
    NonEmpty(ReadDir),
}

impl Iterator for DirContents {
    type Item = Result<DirEntry, std::io::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        match self {
            DirContents::Empty => None,
            DirContents::NonEmpty(read_dir) => read_dir.next(),
        }
    }
}

#[cfg(test)]
mod tests;
