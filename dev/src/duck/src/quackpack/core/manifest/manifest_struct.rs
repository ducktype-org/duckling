use crate::{core::Summary, schemas::manifest::Manifest as ManifestSchema};
use std::sync::Arc;

#[derive(Debug, Clone)]
pub struct Manifest {
    inner: Arc<ManifestInner>,
}

impl Manifest {
    pub fn new(
        original_content: String,
        original_schema: ManifestSchema,
        summary: Summary,
        warnings: Vec<String>,
    ) -> Self {
        Self {
            inner: Arc::new(ManifestInner {
                original_content,
                original_schema,
                summary,
                _warnings: warnings,
            }),
        }
    }

    pub fn original_content(&self) -> &str {
        &self.inner.original_content
    }

    pub fn original_schema(&self) -> &ManifestSchema {
        &self.inner.original_schema
    }

    pub fn summary(&self) -> &Summary {
        &self.inner.summary
    }
}

#[derive(Debug)]
struct ManifestInner {
    original_content: String,
    original_schema: ManifestSchema,
    summary: Summary,
    _warnings: Vec<String>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assert_send_sync_manifest() {
        fn assert_send<T: Send>() {}
        fn assert_sync<T: Sync>() {}
        assert_send::<Manifest>();
        assert_sync::<Manifest>();
    }
}
