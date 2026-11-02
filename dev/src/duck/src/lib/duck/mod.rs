// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Main duck driver implementation.
//! Mainly, a home of the [`driver`] module, and [`DuckContext`](util::duck_context::DuckContext) struct.
pub mod driver;
mod main;
pub mod util;
pub mod version;

pub use main::{main, setup_logger};
