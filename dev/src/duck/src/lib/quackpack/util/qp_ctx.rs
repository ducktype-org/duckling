//! Trait extension used in parsing manifests.
use url::Url;

use crate::{DuckCtx, QuackResult, QuackResultContext, quackpack::core::fetcher};

/// Trait extension for methods used while parsing manifests.
pub trait QpCtx {
    /// Get the default registry URL.
    fn registry_url(&self) -> QuackResult<Url>;
}

impl QpCtx for DuckCtx {
    fn registry_url(&self) -> QuackResult<Url> {
        let url = self
            .duck_cfg()
            .yaml_config()
            .get_str("registry.url")?
            .unwrap_or(fetcher::Fetcher::DEFAULT_REGISTRY_URL);
        Url::parse(url).with_context(|| format!("`{url}` is not a valid URL"))
    }
}
