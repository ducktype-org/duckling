// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for iterating over directory contents.

use std::{
    fs::{DirEntry, ReadDir},
    path::Path,
};

use crate::QuackResult;

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

/// Returns iterator over files in a directory.
/// If the directory does not exist, returns an empty iterator.
pub fn create_dir_iterator(path: &Path) -> QuackResult<DirContents> {
    match path.read_dir() {
        Ok(read_dir) => Ok(DirContents::NonEmpty(read_dir)),
        Err(e) => match e.kind() {
            std::io::ErrorKind::NotFound => Ok(DirContents::Empty),
            _ => Err(e.into()),
        },
    }
}
