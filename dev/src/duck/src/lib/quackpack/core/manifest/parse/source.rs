//! Parse a dependency source.
//!
//! This is the hardest (and most crucial) part of the parsing process.
use std::path::{Path, PathBuf};

use itertools::Itertools;
use tracing::debug;
use url::Url;

use super::{Scope, ScopeGuard};
use crate::quackpack::core::{GitReference, Source};
use crate::quackpack::schemas::manifest::{
    Dependency as DependencySchema, DependencySource as SourceSchema, DetailedSource,
};
use crate::quackpack::util::to_url::ToUrl;
use crate::util::error::MessageError;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_err};

/// Parse given [`DependencySchema`] into [`Source`].
///
/// It's the most important and complex part of current parsing process.
#[tracing::instrument(skip_all)]
pub(crate) fn parse(
    schema: &DependencySchema,
    package_root: &Path,
    ctx: &DuckContext,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Source> {
    debug!(?schema);
    let Some(ref source) = schema.source else {
        debug!("missing the source, falling back to the default registry");
        if schema.version.is_some() {
            let url = ctx.duck_cfg().registry_url()?;
            return Ok(Source::for_registry(url));
        }
        scope.disarm_and_pop();
        return Err(QuackError::hint(
            "provide one of the `version` or the `source` fields",
        ))
        .context(format!(
            "couldn't determine the source of the dependency `{}`",
            scope.format()
        ));
    };
    if schema.version.is_some() && source.has_local() {
        scope.disarm_and_pop();
        let formatted = scope.format();
        return Err(QuackError::hint(format!(
            "remove one of the fields `{formatted}.version` or `{formatted}.source.path`"
        )))
        .context(format!(
            "couldn't determine the type of the dependency `{formatted}`"
        ));
    }
    let source = match source {
        SourceSchema::Simple(registry_url) => {
            let url = registry_url.as_str().to_url()?;
            return Ok(Source::for_registry(url));
        }
        SourceSchema::Detailed(detailed_source) => detailed_source,
    };
    let source = match (
        source.registry_url.as_ref(),
        source.path.as_ref(),
        source.git_url.as_ref(),
    ) {
        (None, None, None) => {
            debug!("didn't find any source");
            check_no_git(source, &mut scope)?;
            check_no_local(source, &mut scope)?;
            check_no_registry(source, &mut scope)?;
            if schema.version.is_some() {
                debug!("...but has a version, assuming registry source");
                let url = ctx.duck_cfg().registry_url()?;
                Source::for_registry(url)
            } else {
                scope.pop();
                return Err(QuackError::hint(
                    "provide one of the fields `version` or the `source`",
                ))
                .context(format!(
                    "couldn't determine the source of the dependency `{}`",
                    scope.format()
                ));
            }
        }
        (Some(registry_url), None, None) => {
            debug!("found a registry source");
            check_no_git(source, &mut scope)?;
            check_no_local(source, &mut scope)?;
            let url = registry_url.as_str().to_url()?;
            Source::for_registry(url)
        }
        (None, Some(root), None) => {
            debug!("found a local path source");
            check_no_git(source, &mut scope)?;
            check_no_registry(source, &mut scope)?;
            debug!(path_in_manifest = %root.display(), "path specified in the manifest");
            let dir_root = resolve_path_maybe_relative_to_dir(root, package_root, ctx)?;
            Source::for_local(&dir_root)?
        }
        (None, None, Some(manifest_git_url)) => {
            debug!("found a git source");
            check_no_local(source, &mut scope)?;
            check_no_registry(source, &mut scope)?;
            let reference = resolve_git_reference(source, &scope)?;
            let git_url = parse_git_url(manifest_git_url, package_root, ctx)?;
            Source::for_git(git_url, reference)
        }
        (None, Some(_), Some(_)) => {
            scope.pop();
            return Err(make_could_not_determine_error(&scope, ["path", "git-url"]));
        }
        (Some(_), None, Some(_)) => {
            scope.pop();
            return Err(make_could_not_determine_error(
                &scope,
                ["registry-url", "git-url"],
            ));
        }
        (Some(_), Some(_), None) => {
            scope.pop();
            return Err(make_could_not_determine_error(
                &scope,
                ["registry-url", "path"],
            ));
        }
        (Some(_), Some(_), Some(_)) => {
            scope.pop();
            return Err(make_could_not_determine_error(
                &scope,
                ["registry-url", "path", "git-url"],
            ));
        }
    };
    Ok(source)
}

#[track_caller]
/// Helper for creating repeated "couldn't determine the source of the dependency" errors.
fn make_could_not_determine_error<const N: usize>(
    scope: &Scope,
    fields: [&'static str; N],
) -> QuackError {
    assert!(N == 2 || N == 3, "implementation relies on it, got N={N}");
    let source = fields
        .iter()
        .map(|field| format!("`source.{}`", field))
        .collect::<Vec<_>>();
    let hint_text = if N == 2 {
        format!("remove one of the fields {} or {}", source[0], source[1])
    } else {
        format!(
            "leave only one of the fields: {}, {}, or {}",
            source[0], source[1], source[2]
        )
    };
    QuackError::hint(hint_text).context(MessageError(
        format!(
            "couldn't determine the source of the dependency `{}`",
            scope.format()
        )
        .into(),
    ))
}

/// Check, that `source` doesn't contain any fields belonging to the [`Git`] source.
#[tracing::instrument(skip_all)]
fn check_no_git(source: &DetailedSource, scope: &mut ScopeGuard<'_>) -> QuackResult<()> {
    let fields = [
        (source.git_url.as_ref(), "git-url"),
        (source.tag.as_ref(), "tag"),
        (source.branch.as_ref(), "branch"),
        (source.commit.as_ref(), "commit"),
    ];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.disarm_and_pop();
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a git dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Check, that `source` doesn't contain any fields belonging to the [`Local`] source.
#[tracing::instrument(skip_all)]
fn check_no_local(source: &DetailedSource, scope: &mut ScopeGuard<'_>) -> QuackResult<()> {
    let fields = [(source.path.as_ref(), "path")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.disarm_and_pop();
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a local dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Check, that `source` doesn't contain any fields belonging to the [`Registry`] source.
#[tracing::instrument(skip_all)]
fn check_no_registry(source: &DetailedSource, scope: &mut ScopeGuard<'_>) -> QuackResult<()> {
    let fields = [(source.registry_url.as_ref(), "registry-url")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.disarm_and_pop();
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a registry dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Resolve [`BranchOrTag`] from the given `source`.
#[tracing::instrument(skip_all)]
fn resolve_git_reference(source: &DetailedSource, scope: &Scope) -> QuackResult<GitReference> {
    let mutually_exclusive_fields_error = |names: &'static [&'static str]| {
        let formatted = scope.format();
        let fields = names
            .iter()
            .map(|name| format!("`{formatted}.{name}`"))
            .join(", ");
        qp_err!(
            "the dependency `{formatted}` is a git dependency, but it contains mutually exclusive fields: {fields}"
        )
    };
    match (
        source.branch.as_ref(),
        source.tag.as_ref(),
        source.commit.as_ref(),
    ) {
        (None, None, None) => Ok(GitReference::Default),
        (None, Some(tag), None) => Ok(GitReference::Tag(tag.into())),
        (Some(branch), None, None) => Ok(GitReference::Branch(branch.into())),
        (None, None, Some(commit)) => Ok(GitReference::Rev(commit.into())),
        (Some(_), Some(_), None) => Err(mutually_exclusive_fields_error(&["branch", "tag"])),
        (None, Some(_), Some(_)) => Err(mutually_exclusive_fields_error(&["tag", "commit"])),
        (Some(_), None, Some(_)) => Err(mutually_exclusive_fields_error(&["branch", "commit"])),
        (Some(_), Some(_), Some(_)) => Err(mutually_exclusive_fields_error(&[
            "branch", "tag", "commit",
        ])),
    }
}

/// Resolve absolute path to the local dependency with entry `manifest_root`.
///
/// This functions returns a tuple `(absolute_path, was_expanded_path_relative)`.
pub(super) fn resolve_path_maybe_relative_to_dir(
    path: &Path,
    root: &Path,
    ctx: &DuckContext,
) -> QuackResult<PathBuf> {
    let home = ctx.user_home();
    let Some(home) = home.to_str() else {
        qp_bail!(
            "the user home directory `{}` is not a utf8 path, which is unsupported",
            home.display(),
        )
    };
    let expanded = path.expand_user_with(home)?;
    if expanded.is_absolute() {
        Ok(expanded.to_path_buf())
    } else {
        Ok(root.join(expanded).expand_user_with(home)?.resolve()?)
    }
}

/// Parse a url of a git dependency.
fn parse_git_url(
    manifest_git_url: &str,
    package_root: &Path,
    ctx: &DuckContext,
) -> QuackResult<Url> {
    let git_url = manifest_git_url.to_url();
    let mut err: QuackError = match git_url {
        Ok(parsed) => return Ok(parsed),
        Err(err) => err,
    };
    // We are building error messages from the bottom to the top.
    // If an original URL points to a file, mention it to the user. Also, ignore any errors.
    if let Ok(path) =
        resolve_path_maybe_relative_to_dir(Path::new(manifest_git_url), package_root, ctx)
        && path.exists()
    {
        err = err.add_hint(format!(
            "either change it to a local dependency or change the URL to `file://{}`",
            path.display()
        ));
        err = err.add_note("git dependency points to a file on the disk");
    }
    Err(err)
}
