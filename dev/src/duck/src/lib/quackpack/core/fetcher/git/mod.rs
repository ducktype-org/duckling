// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with Git repositories.

pub mod fast_path;
mod known_hosts;
pub mod libgit2;
#[cfg(test)]
mod tests;
