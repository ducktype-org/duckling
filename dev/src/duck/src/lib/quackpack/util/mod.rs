// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various quackpack-only utilities.
pub mod guards;
pub mod interned_url;
pub mod is_local_file;
pub mod paths;
pub mod str_id;
pub mod to_path_buf;
pub mod to_url;
pub mod with_version;

/// A common message which should be passed to `.expect()`s.
pub const PANIC_MESSAGE: &str = "a thread panic'd, which should not have happened";
