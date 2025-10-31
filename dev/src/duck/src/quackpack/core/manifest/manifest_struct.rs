use crate::{core::Summary, schemas::manifest::Manifest as ManifestSchema};
use std::sync::Arc;

#[derive(Debug)]
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
            inner: Arc::new(ManifestInner::new(
                original_content,
                original_schema,
                summary,
                warnings,
            )),
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

impl ManifestInner {
    fn new(
        original_content: String,
        original_schema: ManifestSchema,
        summary: Summary,
        warnings: Vec<String>,
    ) -> Self {
        Self {
            original_content,
            original_schema,
            summary,
            _warnings: warnings,
        }
    }
}
