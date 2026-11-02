// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Lazy loading of the external subcommands.
use std::cell::OnceCell;
use std::collections::HashMap;
use std::path::PathBuf;

use crate::DuckContext;

#[derive(Debug, Default)]
/// A struct responsible for lazy loading external subcommands.
///
/// Loading external subcommands can take some time: we [`read_dir`] every component of `$PATH`,
/// which is time consuming, especially if a user has lots of components in `$PATH`, or if the
/// directories itself are pretty huge.
///
/// Most of the time, we shouldn't need to go through the external subcommands.
///
/// [`read_dir`]: std::fs::read_dir
pub struct ExternalSubcommands {
    inner: OnceCell<HashMap<String, PathBuf>>,
}

impl ExternalSubcommands {
    /// Lazily load external subcommands.
    ///
    /// This method should be used as rarely and as late as possible.
    pub fn load(&self, ctx: &DuckContext) -> &HashMap<String, PathBuf> {
        self.inner
            .get_or_init(|| super::run::gather_external_subcmds(ctx))
    }
}
