//! Trait extension used in parsing manifests.
use url::Url;

use crate::quackpack::core::fetcher;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Trait extension for methods used while parsing manifests.
pub trait QpContext {
    /// Get the default registry URL.
    fn registry_url(&self) -> QuackResult<Url>;
}

impl QpContext for DuckContext {
    fn registry_url(&self) -> QuackResult<Url> {
        let url = self
            .duck_cfg()
            .yaml_config()
            .get_str("registry.url")?
            .unwrap_or(fetcher::Fetcher::DEFAULT_REGISTRY_URL);
        Url::parse(url).with_context(|| format!("`{url}` is not a valid URL"))
    }
}
