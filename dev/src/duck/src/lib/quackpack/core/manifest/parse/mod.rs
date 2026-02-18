use std::collections::BTreeSet;
use std::path::Path;

use itertools::Itertools;
use rustvil::fs::PathExt;
use tracing::{Level, debug, span};

use crate::quackpack::core::Manifest;
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
    let warnings = create_warnings(&content, &schema, &manifest);
    Ok(Package::new(
        content,
        schema,
        manifest,
        package_root.into(),
        path.into(),
        warnings,
    ))
}

/// Turn YAML string into the [`ManifestSchema`].
/// This function also collects unused items in the [`ManifestSchema`].
fn parse_schema(yaml_content: &str) -> QuackResult<ManifestSchema> {
    let mut unused = BTreeSet::new();
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let mut schema: ManifestSchema = serde_ignored::deserialize(deserializer, |path| {
        unused.insert(concat_unused_path(&path));
    })?;
    schema._unused_keys = unused;
    Ok(schema)
}

/// Format [`serde_ignored::Path`] as a human readable [`String`].
fn concat_unused_path(path: &serde_ignored::Path<'_>) -> String {
    use serde_ignored::Path;

    match *path {
        Path::Root => String::new(),
        Path::Seq { parent, index } => {
            let mut parent_concat = concat_unused_path(parent);
            if !parent_concat.is_empty() {
                parent_concat.push('.');
            }
            parent_concat.push_str(&format!("<index:{index}>"));
            parent_concat
        }
        Path::Map { parent, ref key } => {
            let mut parent_concat = concat_unused_path(parent);
            if !parent_concat.is_empty() {
                parent_concat.push('.');
            }
            parent_concat.push_str(key);
            parent_concat
        }
        Path::Some { parent }
        | Path::NewtypeStruct { parent }
        | Path::NewtypeVariant { parent } => concat_unused_path(parent),
    }
}

/// Create any manifest-related warnings.
/// Right now this function only warns about unused items.
fn create_warnings(
    _original_yaml: &str,
    schema: &ManifestSchema,
    _summary: &Manifest,
) -> Vec<String> {
    schema
        ._unused_keys
        .iter()
        .map(|key| format!("Unused manifest key: `{key}`"))
        .collect()
}
