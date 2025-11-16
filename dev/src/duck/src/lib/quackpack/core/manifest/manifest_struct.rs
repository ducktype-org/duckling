use crate::{
    DuckCtx,
    quackpack::{core::Summary, schemas::manifest::Manifest as ManifestSchema},
};
use std::sync::Arc;

#[derive(Debug, Clone)]
/// High-level abstraction about a manifest of a package we're working on.
pub struct Manifest {
    inner: Arc<ManifestInner>,
}

impl Manifest {
    /// Create a new manifest.
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
                warnings,
            }),
        }
    }

    /// Get the original manifest file content.
    pub fn original_content(&self) -> &str {
        &self.inner.original_content
    }

    /// Get the original parsed manifest schema.
    pub fn original_schema(&self) -> &ManifestSchema {
        &self.inner.original_schema
    }

    /// Get the high-level summary of the manifest.
    pub fn summary(&self) -> &Summary {
        &self.inner.summary
    }

    pub fn emit_warnings(&self, ctx: &DuckCtx) {
        for warning in self.inner.warnings.iter() {
            ctx.error_console().warning(warning);
        }
    }
}

#[derive(Debug)]
struct ManifestInner {
    original_content: String,
    original_schema: ManifestSchema,
    summary: Summary,
    warnings: Vec<String>,
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
