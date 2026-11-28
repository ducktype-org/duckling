// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Filesystem helpers: path operations with readable errors, file locks and directory listing.
mod dir_iterator;
mod file_locks;
mod path_ops;
mod source_file_visitor;

pub use dir_iterator::{DirContents, create_dir_iterator};
pub use file_locks::{FileLockManager, LockedFile};
pub use path_ops::*;
pub use source_file_visitor::*;
