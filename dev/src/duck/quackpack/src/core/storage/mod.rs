// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

pub use ops::*;
pub mod venv_id;

#[cfg(test)]
mod tests;
