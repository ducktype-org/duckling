//! Trait extension used in parsing manifests.
use url::Url;

use crate::quackpack::core::fetcher;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackResult};

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
        url.to_url()
    }
}
