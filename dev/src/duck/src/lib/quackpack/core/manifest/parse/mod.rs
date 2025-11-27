use std::collections::BTreeSet;
use std::path::Path;

use anyhow::Context;
use itertools::Itertools;
use rustvil::fs::PathExt;
use tracing::{Level, debug, span};

use crate::quackpack::core::Manifest;
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::{InternalError, QpCtx, StrId};
use crate::{QuackResult, quackpack::core::Package};

mod dependency;
mod manifest;
mod source;

#[cfg(test)]
mod tests;

pub fn parse_manifest(path: &Path, ctx: QpCtx<'_>) -> QuackResult<Package> {
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
pub(crate) struct Scope {
    inner: Vec<StrId>,
}

impl Scope {
    pub fn new() -> Self {
        Scope { inner: Vec::new() }
    }

    pub fn push(&mut self, name: StrId) {
        self.inner.push(name)
    }

    pub fn pop(&mut self) -> Option<StrId> {
        self.inner.pop()
    }

    pub fn format(&self) -> String {
        self.inner.iter().join(".")
    }
}

fn parse_inner(path: &Path, ctx: QpCtx<'_>) -> QuackResult<Package> {
    let package_root = path
        .parent()
        .ok_or_else(|| InternalError::from("the manifest path has no parent"))?;
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
        warnings,
    ))
}

fn parse_schema(yaml_content: &str) -> QuackResult<ManifestSchema> {
    let mut unused = BTreeSet::new();
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let mut schema: ManifestSchema = serde_ignored::deserialize(deserializer, |path| {
        unused.insert(concat_unused_path(&path));
    })?;
    schema._unused_keys = unused;
    Ok(schema)
}

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
