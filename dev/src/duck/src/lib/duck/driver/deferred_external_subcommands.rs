//! Lazy loading of the external subcommands.
use std::cell::OnceCell;
use std::collections::HashMap;
use std::path::PathBuf;

use crate::DuckContext;

#[derive(Debug, Default)]
pub struct DeferredExternalSubcommands {
    inner: OnceCell<HashMap<String, PathBuf>>,
}

impl DeferredExternalSubcommands {
    /// Lazily load external subcommands.
    pub fn load(&self, ctx: &DuckContext) -> &HashMap<String, PathBuf> {
        self.inner
            .get_or_init(|| super::run::gather_external_subcmds(ctx))
    }
}
