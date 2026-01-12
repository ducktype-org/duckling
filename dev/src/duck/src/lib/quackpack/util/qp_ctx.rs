use std::path::Path;

use url::Url;

use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId,
    duck::util::{duck_home::DuckHome, terminal::Terminal},
    static_str_id,
};

#[derive(Debug)]
/// An extension of DuckCtx with quackpack-specific functionalities.
pub struct QpCtx<'duck> {
    inner: &'duck DuckCtx,
}

impl<'duck> QpCtx<'duck> {
    /// Creates QpCtx from DuckCtx.
    pub fn new(duck_ctx: &'duck DuckCtx) -> Self {
        Self { inner: duck_ctx }
    }

    /// Retrieves the underlying DuckCtx's console.
    pub fn console(&self) -> &Terminal {
        self.inner.console()
    }

    /// Retrieves the underlying DuckCtx's error console.
    pub fn error_console(&self) -> &Terminal {
        self.inner.error_console()
    }

    pub fn registry_url(&self) -> QuackResult<Url> {
        let url = self
            .inner
            .duck_cfg()
            .toml_config()
            .get_str("registry.url")?
            .map(StrId::from)
            // @TODO: #1548 Move this to the fetcher module
            .unwrap_or_else(|| static_str_id!("http://localhost:9001"));
        Url::parse(&url).with_context(|| format!("`{url}` is not a valid URL"))
    }

    pub fn cwd(&self) -> &Path {
        self.inner.cwd()
    }

    pub fn user_home(&self) -> &Path {
        self.inner.user_home()
    }

    pub fn duck_home(&self) -> &DuckHome {
        self.inner.duck_home()
    }
}
