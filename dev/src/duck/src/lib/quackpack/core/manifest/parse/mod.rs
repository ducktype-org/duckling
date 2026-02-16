use std::path::Path;

use itertools::Itertools;
use rustvil::fs::PathExt;
use serde::Deserialize;
use tracing::{Level, debug, span};

use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::{QpCtx, QuackResultContext, StrId, qp_internal};
use crate::{QuackResult, quackpack::core::Package};

mod dependency;
mod manifest;
mod source;

#[cfg(test)]
mod tests;

/// Parse a manifest at a given `path`.
///
/// This function does not expand tildes or resolves paths: its callers responsibility to do that.
///
/// Entire parsing is done in three steps:
/// 1. Read the entire YAML string.
/// 2. Turn that string into [`ManifestSchema`].
/// 3. Parse [`ManifestSchema`] into [`Manifest`].
pub fn parse_manifest(path: &Path, ctx: &QpCtx<'_>) -> QuackResult<Package> {
    let span = span!(Level::DEBUG, "manifest", path = %path.display());
    let _guard = span.enter();
    debug!("starting parsing...");
    parse_inner(path, ctx).with_context(|| {
        format!(
            "when trying to parse the user manifest at `{}`",
            path.display()
        )
    })
}

#[derive(Debug)]
/// Scope representing, which item in [`ManifestSchema`] we are currently working on.
pub(crate) struct Scope {
    inner: Vec<StrId>,
}

impl Scope {
    /// Create a new [`Scope`].
    pub fn new() -> Self {
        Scope { inner: Vec::new() }
    }

    /// Push a `name` onto this [`Scope`].
    pub fn push(&mut self, name: StrId) {
        self.inner.push(name)
    }

    /// Pop last item from this [`Scope`].
    pub fn pop(&mut self) -> Option<StrId> {
        self.inner.pop()
    }

    /// Turn this [`Scope`] into a human friendly [`String`].
    pub fn format(&self) -> String {
        self.inner.iter().join(".")
    }
}

/// Helper for [`parse_manifest`].
fn parse_inner(path: &Path, ctx: &QpCtx<'_>) -> QuackResult<Package> {
    let package_root = path
        .parent()
        .ok_or_else(|| qp_internal!("the manifest path has no parent"))?;
    let content = path
        .read_to_string()
        .context("failed to read the manifest's content")?;
    let schema = parse_schema(&content)?;
    let manifest = manifest::parse(&schema, package_root, ctx)?;
    Ok(Package::new(
        content,
        schema,
        manifest,
        package_root.into(),
        path.into(),
    ))
}

/// Turn YAML string into the [`ManifestSchema`].
/// This function also collects unused items in the [`ManifestSchema`].
fn parse_schema(yaml_content: &str) -> QuackResult<ManifestSchema> {
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let schema = ManifestSchema::deserialize(deserializer)?;
    Ok(schema)
}
