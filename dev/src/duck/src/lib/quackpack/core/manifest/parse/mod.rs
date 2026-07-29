//! Main entry to parsing a manifest at the given path.
use std::ops::{Deref, DerefMut};
use std::path::Path;

use itertools::Itertools;
use serde::Deserialize;
use tracing::debug;

use crate::quackpack::core::Package;
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_internal};

mod dependency;
mod frontmatter;
mod manifest;
mod source;

pub use frontmatter::{capture_frontmatter, parse_frontmatter};
pub(crate) use manifest::parse;

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

/// Different utilites in which manifests occur.
/// Used to perform appropriate checks on presence/absence of certain fields.
pub enum ParseMode {
    Package,
    FrontMatter,
}

#[derive(Debug)]
/// Scope representing, which item in [`ManifestSchema`] we are currently working on.
pub(crate) struct Scope {
    inner: Vec<String>,
}

impl Scope {
    /// Create a new [`Scope`].
    pub fn new() -> Self {
        Scope { inner: Vec::new() }
    }

    /// Push a `name` onto this [`Scope`].
    pub fn push(&mut self, name: String) -> ScopeGuard<'_> {
        ScopeGuard::new(self, name)
    }

    /// Pop last item from this [`Scope`].
    pub fn pop(&mut self) -> String {
        self.inner.pop().unwrap()
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

/// A [`Scope`] guard.
/// Will automatically `pop` when going out of scope, unless disarmed.
pub(crate) struct ScopeGuard<'scope> {
    scope: &'scope mut Scope,
    armed: bool,
}

impl<'scope> ScopeGuard<'scope> {
    pub fn new(scope: &'scope mut Scope, name: String) -> Self {
        scope.inner.push(name);
        Self { scope, armed: true }
    }

    /// Disarm this guard.
    /// Dropping it will have no effect.
    pub fn disarm(&mut self) {
        self.armed = false;
    }

    /// Disarm this guard, and return a result of a manual pop.
    pub fn disarm_and_pop(&mut self) -> String {
        self.disarm();
        self.pop()
    }
}

impl Drop for ScopeGuard<'_> {
    fn drop(&mut self) {
        if self.armed {
            self.scope.inner.pop();
        }
    }
}

impl Deref for ScopeGuard<'_> {
    type Target = Scope;

    fn deref(&self) -> &Self::Target {
        self.scope
    }
}

impl DerefMut for ScopeGuard<'_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.scope
    }
}

/// Helper for [`parse_manifest`].
fn parse_inner(path: &Path, ctx: &DuckContext) -> QuackResult<Package> {
    let package_root = path
        .parent()
        .ok_or_else(|| qp_internal!("the manifest path has no parent"))?;
    let content = path.read_to_string()?;
    let schema = parse_schema(&content)?;
    let manifest = manifest::parse(&schema, package_root, ParseMode::Package, ctx)?;
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
pub fn parse_schema(yaml_content: &str) -> QuackResult<ManifestSchema> {
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let schema = ManifestSchema::deserialize(deserializer)?;
    Ok(schema)
}
