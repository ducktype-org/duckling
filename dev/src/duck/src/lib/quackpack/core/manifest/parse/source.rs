use std::path::{Path, PathBuf};

use tracing::debug;

use rustvil::fs::PathExt;

use super::Scope;
use crate::{
    QpCtx, QuackError, QuackResult, QuackResultContext, qp_bail, qp_internal,
    quackpack::{
        core::{Git, GitRevision, Local, Registry, Source},
        schemas::manifest::{DependencySource as SourceSchema, DetailedSource},
    },
};

use crate::quackpack::schemas::manifest::Dependency as DependencySchema;

/// Parse given [`DependencySchema`] into [`Source`].
///
/// It's the most important and complex part of current parsing process.
pub(crate) fn parse(
    schema: &DependencySchema,
    package_root: &Path,
    ctx: &QpCtx<'_>,
    scope: &mut Scope,
) -> QuackResult<Source> {
    let Some(ref source) = schema.source else {
        debug!("missing the source, falling back to the default registry...?");
        if schema.version.is_some() {
            return Ok(Source::Registry(Registry::new(ctx.registry_url()?)));
        }
        scope.pop();
        return Err(QuackError::new()
            .add_hint("provide one of the `version` or the `source` fields")
            .context(format!(
                "couldn't determine the source of the dependency `{}`",
                scope.format()
            )));
    };
    if schema.version.is_some() && source.has_local() {
        scope.pop();
        let formatted = scope.format();
        return Err(QuackError::new()
            .add_hint(format!(
                "remove one of the fields `{formatted}.version` or `{formatted}.source.path`"
            ))
            .context(format!(
                "couldn't determine the type of the dependency `{formatted}`"
            )));
    }
    let source = match source {
        SourceSchema::Simple(registry_url) => {
            return Ok(Registry::new(registry_url.into()).into());
        }
        SourceSchema::Detailed(detailed_source) => detailed_source,
    };
    let source: Source = match (
        source.registry_url.as_ref(),
        source.path.as_ref(),
        source.git_url.as_ref(),
    ) {
        (None, None, None) => {
            debug!("didn't find any source");
            check_no_git(source, scope)?;
            check_no_local(source, scope)?;
            check_no_registry(source, scope)?;
            if schema.version.is_some() {
                debug!("...but has a version, assuming registry source");
                Registry::new(ctx.registry_url()?).into()
            } else {
                scope.pop();
                return Err(QuackError::new()
                    .add_hint(format!(
                        "provide one of the fields `version` or the `source`"
                    ))
                    .context(format!(
                        "couldn't determine the source of the dependency `{}`",
                        scope.format()
                    )));
            }
        }
        (Some(registry_url), None, None) => {
            debug!("found a registry source");
            check_no_git(source, scope)?;
            check_no_local(source, scope)?;
            Registry::new(registry_url.into()).into()
        }
        (None, Some(root), None) => {
            debug!("found a local path source");
            check_no_git(source, scope)?;
            check_no_registry(source, scope)?;
            debug!("manifest path is `{root}`");
            let (dir_root, was_relative) = resolve_local_dep_root(root, package_root, ctx)?;
            Local::new(dir_root, root.into(), was_relative).into()
        }
        (None, None, Some(git_url)) => {
            debug!("found a git source");
            check_no_local(source, scope)?;
            check_no_registry(source, scope)?;
            let rev = resolve_git_rev(source, scope)?;
            Git::new(
                git_url.into(),
                rev,
                source.commit.as_ref().map(<&String>::into),
            )
            .into()
        }
        (None, Some(_), Some(_)) => {
            scope.pop();
            qp_bail!(make_could_not_determine_error(scope, ["path", "git_url"]))
        }
        (Some(_), None, Some(_)) => {
            scope.pop();
            qp_bail!(make_could_not_determine_error(
                scope,
                ["registry_url", "git_url"]
            ))
        }
        (Some(_), Some(_), None) => {
            scope.pop();
            qp_bail!(make_could_not_determine_error(
                scope,
                ["registry_url", "path"]
            ))
        }
        (Some(_), Some(_), Some(_)) => {
            scope.pop();
            qp_bail!(make_could_not_determine_error(
                scope,
                ["registry_url", "path", "git_url"]
            ))
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
    assert!(N == 2 || N == 3, "implementation relies on it");
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
    QuackError::new().add_hint(hint_text).context(format!(
        "couldn't determine the source of the dependency `{}`",
        scope.format()
    ))
}

/// Check, that `source` doesn't contain any fields belonging to the [`Git`] source.
fn check_no_git(source: &DetailedSource, scope: &mut Scope) -> QuackResult<()> {
    let fields = [
        (source.git_url.as_ref(), "git_url"),
        (source.tag.as_ref(), "tag"),
        (source.branch.as_ref(), "branch"),
        (source.commit.as_ref(), "commit"),
    ];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.pop();
            let old = old.ok_or_else(|| {
                qp_internal!("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a git dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Check, that `source` doesn't contain any fields belonging to the [`Local`] source.
fn check_no_local(source: &DetailedSource, scope: &mut Scope) -> QuackResult<()> {
    let fields = [(source.path.as_ref(), "path")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.pop();
            let old = old.ok_or_else(|| {
                qp_internal!("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a local dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Check, that `source` doesn't contain any fields belonging to the [`Registry`] source.
fn check_no_registry(source: &DetailedSource, scope: &mut Scope) -> QuackResult<()> {
    let fields = [(source.registry_url.as_ref(), "registry_url")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.pop();
            let old = old.ok_or_else(|| {
                qp_internal!("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            qp_bail!(
                "expected the dependency `{formatted}` to not be a registry dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

/// Resolve [`GitRevision`] from the given `source`.
fn resolve_git_rev(source: &DetailedSource, scope: &Scope) -> QuackResult<GitRevision> {
    match (source.branch.as_ref(), source.tag.as_ref()) {
        (None, None) => Ok(GitRevision::Main),
        (None, Some(tag)) => Ok(GitRevision::Tag(tag.into())),
        (Some(branch), None) => Ok(GitRevision::Branch(branch.into())),
        (Some(_), Some(_)) => {
            let formatted = scope.format();
            qp_bail!(
                "the dependency `{formatted}` is a git dependency, but it contains mutually exclusive fields: `{formatted}.branch`, `{formatted}.commit`"
            );
        }
    }
}

/// Resolve absolute path to the local dependency with entry `manifest_root`.
///
/// This functions returns a tuple `(absolute_path, was_expanded_path_relative)`.
fn resolve_local_dep_root(
    manifest_root: &str,
    package_root: &Path,
    ctx: &QpCtx<'_>,
) -> QuackResult<(PathBuf, bool)> {
    let home = ctx.user_home();
    let Some(home) = home.to_str() else {
        qp_bail!(
            "the user home directory `{}` is not a utf-8 path, which is unsupported",
            home.display()
        )
    };
    let expanded = Path::new(manifest_root)
        .expand_user_with(home)
        .with_context(|| format!("failed to expand the tildes from the path `{manifest_root}`"))?;
    if expanded.is_absolute() {
        Ok((expanded.to_path_buf(), false))
    } else {
        Ok((
            package_root
                .join(expanded)
                .expand_user_with(home)?
                .resolve()?,
            true,
        ))
    }
}
