use std::path::{Path, PathBuf};

use serde::Deserialize;

use crate::quackpack::core::manifest::parse::manifest::parse_profiles;
use crate::{
    DuckContext, QuackResult, qp_bail, qp_err,
    quackpack::{
        core::{
            Scope,
            manifest::{frontmatter::FrontMatter, parse::dependency},
        },
        schemas::frontmatter::FrontMatter as FrontMatterSchema,
    },
    util::path_ops_ext::PathOpsExt,
};

#[tracing::instrument(skip(ctx))]
pub fn parse_frontmatter(path: &Path, ctx: &DuckContext) -> QuackResult<FrontMatter> {
    let mut paths = vec![];
    let content = path.read_to_string()?;
    let schema = parse_schema(&content)?;
    parse_frontmatter_impl(&schema, path, &mut paths, ctx)
}

fn parse_schema(yaml_content: &str) -> QuackResult<FrontMatterSchema> {
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let schema = FrontMatterSchema::deserialize(deserializer)?;
    Ok(schema)
}

/// Parse [`FrontMatter`] from given [`FrontMatterSchema`].
#[tracing::instrument(skip_all)]
pub(crate) fn parse_frontmatter_impl(
    schema: &FrontMatterSchema,
    root: &Path,
    paths: &mut Vec<PathBuf>,
    ctx: &DuckContext,
) -> QuackResult<FrontMatter> {
    if paths.iter().any(|p| p.as_path() == root) {
        let displayable: Vec<String> = paths.iter().map(|p| p.display().to_string()).collect();
        qp_bail!(
            "Encountered a cycle of frontmatters imports: {} -> {}",
            displayable.join(" -> "),
            root.display()
        );
    }
    paths.push(root.to_path_buf());
    if let Some(ref path) = schema.import {
        if schema.dependencies.is_some()
            || schema.dev_dependencies.is_some()
            || schema.profiles.is_some()
        {
            let mut err = qp_err!(
                "the frontmatter at `{}` imports another frontmatter but also specifies one of the fields [`dependencies`, `dev-dependencies`, `profiles`]",
                root.display()
            );
            err = err.add_hint(
                "Either remove the `import` field or all the other fields from the frontmatter",
            );
            qp_bail!(err);
        }
        return parse_frontmatter(path, ctx);
    }

    let mut scope = Scope::new();
    let guard = scope.push("dependencies".into());
    let dependencies = dependency::parse(schema.dependencies.as_ref(), root, ctx, guard)?;

    let guard = scope.push("dev-dependencies".into());
    let dev_deps = dependency::parse(schema.dev_dependencies.as_ref(), root, ctx, guard)?;

    let guard = scope.push("profiles".into());
    let profiles = parse_profiles(schema.profiles.as_ref(), guard)?;

    Ok(FrontMatter::new(dependencies, dev_deps, profiles))
}
