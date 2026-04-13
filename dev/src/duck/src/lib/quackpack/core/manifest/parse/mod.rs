//! Main entry to parsing a manifest at the given path.
use std::path::Path;

use itertools::Itertools;
use serde::Deserialize;
use tracing::debug;

use crate::quackpack::core::Package;
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_internal};

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
#[tracing::instrument(skip(ctx))]
pub fn parse_manifest(path: &Path, ctx: &DuckContext) -> QuackResult<Package> {
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

    /// A helper for creating common context messages.
    pub fn make_context_string(&self) -> String {
        format!("when parsing the field `{}`", self.format())
    }
}

/// Helper for [`parse_manifest`].
fn parse_inner(path: &Path, ctx: &DuckContext) -> QuackResult<Package> {
    let package_root = path
        .parent()
        .ok_or_else(|| qp_internal!("the manifest path has no parent"))?;
    let content = path.read_to_string()?;
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
