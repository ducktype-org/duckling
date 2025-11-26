use std::path::Path;

use anyhow::{Context, bail};
use tracing::debug;

use rustvil::fs::PathExt;

use super::Scope;
use crate::{
    InternalError, QpCtx, QuackResult,
    quackpack::{
        core::{Git, GitRevision, Local, Registry, Source},
        schemas::manifest::{DependencySource as SourceSchema, DetailedSource},
    },
};

use crate::quackpack::schemas::manifest::Dependency as DependencySchema;
pub(crate) fn parse(
    schema: &DependencySchema,
    package_root: &Path,
    ctx: QpCtx<'_>,
    scope: &mut Scope,
) -> QuackResult<Source> {
    let Some(ref source) = schema.source else {
        debug!("missing the source, falling back to the default registry...?");
        if schema.version.is_some() {
            return Ok(Source::Registry(Registry::new(ctx.registry_url()?)));
        }
        scope.pop();
        bail!(
            "couldn't determine the source of the dependency `{}`\n\
            hint: provide one of the `version` or the `source` fields",
            scope.format()
        )
    };
    if schema.version.is_some() && source.has_local() {
        scope.pop();
        let formatted = scope.format();
        bail!(
            "couldn't determine the type of the dependency `{formatted}`\n\
            hint: remove one of the fields `{formatted}.version` or `{formatted}.source.path`"
        )
    }
    // `let-else` doesn't work, somehow rust screws up pattern matching.
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
                bail!(
                    "couldn't determine the source of the dependency `{}`\n\
                    hint: provide one of the fields `version` or the `source`",
                    scope.format()
                )
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
            let home = ctx.user_home();
            let Some(home) = home.to_str() else {
                bail!(
                    "the user home directory `{}` is not a utf-8 path, which is unsupported",
                    home.display()
                )
            };
            let expanded = Path::new(root)
                .expand_user_with(home)
                .with_context(|| format!("failed to expand the tildes from the path `{root}`"))?;
            if expanded.is_absolute() {
                Local::new(expanded.to_path_buf(), root.into()).into()
            } else {
                let dir_root = package_root.join(expanded);
                Local::new(dir_root.expand_user_with(home)?.resolve()?, root.into()).into()
            }
        }
        (None, None, Some(git_url)) => {
            debug!("found a git source");
            check_no_local(source, scope)?;
            check_no_registry(source, scope)?;
            debug!(?scope);
            let rev = match (source.branch.as_ref(), source.tag.as_ref()) {
                (None, None) => GitRevision::Main,
                (None, Some(tag)) => GitRevision::Tag(tag.into()),
                (Some(branch), None) => GitRevision::Branch(branch.into()),
                (Some(_), Some(_)) => {
                    let formatted = scope.format();
                    bail!(
                        "the dependency `{formatted}` is a git dependency, but it contains mutually exclusive fields: \
                        `{formatted}.branch`, `{formatted}.commit`"
                    );
                }
            };
            Git::new(
                git_url.into(),
                rev,
                source.commit.as_ref().map(|commit| commit.into()),
            )
            .into()
        }
        (None, Some(_), Some(_)) => {
            scope.pop();
            bail!(
                "couldn't determine the source of the dependency `{}`\n\
                hint: remove one of the fields `source.path` or `source.git_url`",
                scope.format()
            )
        }
        (Some(_), None, Some(_)) => {
            scope.pop();
            bail!(
                "couldn't determine the source of the dependency `{}`\n\
                hint: remove one of the fields `source.registry_url` or `source.git_url`",
                scope.format()
            )
        }
        (Some(_), Some(_), None) => {
            scope.pop();
            bail!(
                "couldn't determine the source of the dependency `{}`\n\
                hint: remove one of the fields `source.registry_url` or `source.path`",
                scope.format()
            )
        }
        (Some(_), Some(_), Some(_)) => {
            scope.pop();
            bail!(
                "couldn't determine the source of the dependency `{}`\n\
                hint: leave only one of the fields: \
                `source.registry_url`, `source.path`, or `source.git_url`",
                scope.format()
            )
        }
    };
    Ok(source)
}

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
                InternalError::from("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            bail!(
                "expected the dependency `{formatted}` to not be a git dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

fn check_no_local(source: &DetailedSource, scope: &mut Scope) -> QuackResult<()> {
    let fields = [(source.path.as_ref(), "path")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.pop();
            let old = old.ok_or_else(|| {
                InternalError::from("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            bail!(
                "expected the dependency `{formatted}` to not be a local dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}

fn check_no_registry(source: &DetailedSource, scope: &mut Scope) -> QuackResult<()> {
    let fields = [(source.registry_url.as_ref(), "registry_url")];
    for (field, name) in fields {
        if field.is_some() {
            let old = scope.pop();
            let old = old.ok_or_else(|| {
                InternalError::from("when parsing the manifest scope should always be non-empty")
            })?;
            let formatted = scope.format();
            bail!(
                "expected the dependency `{formatted}` to not be a registry dependency, but the field `{formatted}.{old}.{name}` is set"
            )
        }
    }
    Ok(())
}
