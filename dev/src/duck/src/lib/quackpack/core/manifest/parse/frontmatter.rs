use std::path::Path;
use std::sync::LazyLock;

use regex::Regex;
use serde::Deserialize;
use tracing::debug;

use crate::QuackResultContext;
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

pub static FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new("<frontmatter>([^]*)</frontmatter>").unwrap());

/// Deserialize a script frontmatter.
#[tracing::instrument(skip(ctx))]
pub fn parse_frontmatter(path: &Path, ctx: &DuckContext) -> QuackResult<Option<FrontMatter>> {
    let Some(schema) = generate_schema_script(path)? else {
        return Ok(None);
    };
    Some(parse_schema_script(path, &schema, ctx)).transpose()
}

/// Generate a [`FrontMatterSchema`] from the script's contents.
fn generate_schema_script(path: &Path) -> QuackResult<Option<FrontMatterSchema>> {
    let content = path.read_to_string()?;
    let Some(captures) = FRONTMATTER_REGEX.captures(&content) else {
        return Ok(None);
    };
    let frontmatter_content = captures.get(1).unwrap().as_str();
    Some(generate_schema(frontmatter_content)).transpose()
}

/// Helper for [`parse_schema_script`].
/// Generate [`FrontMatterSchema`] from a yaml file, which was imported by another frontmatter.
fn generate_schema_yaml(path: &Path) -> QuackResult<FrontMatterSchema> {
    let content = path.read_to_string()?;
    generate_schema(&content)
}

/// Helper for [`generate_schema_script`] and [`generate_schema_yaml`].
/// Generate [`FrontMatterSchema`] from a string.
fn generate_schema(yaml_content: &str) -> QuackResult<FrontMatterSchema> {
    let deserializer = serde_yaml_ng::Deserializer::from_str(yaml_content);
    let schema = FrontMatterSchema::deserialize(deserializer)?;
    Ok(schema)
}

/// Generate [`FrontMatter`] from [`FrontMatterSchema`] of a script.
#[tracing::instrument(skip(schema, ctx))]
fn parse_schema_script(
    script_path: &Path,
    schema: &FrontMatterSchema,
    ctx: &DuckContext,
) -> QuackResult<FrontMatter> {
    if let Some(ref import) = schema.import {
        debug!(
            "Script at `{}` imports frontmatter at `{}`",
            script_path.display(),
            import.display()
        );
        if !schema.is_just_import() {
            let mut err = qp_err!(
                "Script at `{}` imports a frontmatter but also specifies some of the frontmatter fields",
                script_path.display()
            );
            err = err.add_hint("Either remove the `import` field or all the other fields");
            qp_bail!(err);
        }
        let imported_schema = generate_schema_yaml(import).context(format!(
            "While reading the frontmatter imported by `{}`",
            script_path.display()
        ))?;
        return parse_schema_yaml(import, &imported_schema, ctx).context(format!(
            "While parsing the frontmatter imported by `{}`",
            script_path.display()
        ));
    }
    parse_schema(script_path, schema, ctx)
}

/// Helper for [`parse_schema_script`].
/// Generate [`FrontMatter`] from [`FrontMatterSchema`] of an imported yaml file.
#[tracing::instrument(skip(schema, ctx))]
fn parse_schema_yaml(
    yaml_file_path: &Path,
    schema: &FrontMatterSchema,
    ctx: &DuckContext,
) -> QuackResult<FrontMatter> {
    if schema.import.is_some() {
        qp_bail!(
            "Imported frontmatter at `{}` also wants to import, which is prohibited",
            yaml_file_path.display()
        );
    }
    parse_schema(yaml_file_path, schema, ctx)
}

/// Helper for [`parse_schema_script`] and [`parse_schema_yaml`].
/// Generate [`FrontMatter`] from [`FrontMatterSchema`], disregarding any import.
#[tracing::instrument(skip(schema, ctx))]
fn parse_schema(
    path: &Path,
    schema: &FrontMatterSchema,
    ctx: &DuckContext,
) -> QuackResult<FrontMatter> {
    let mut scope = Scope::new();
    let guard = scope.push("dependencies".into());
    let dependencies = dependency::parse(schema.dependencies.as_ref(), path, ctx, guard)?;

    let guard = scope.push("dev-dependencies".into());
    let dev_dependencies = dependency::parse(schema.dev_dependencies.as_ref(), path, ctx, guard)?;

    let guard = scope.push("profiles".into());
    let profiles = parse_profiles(schema.profiles.as_ref(), guard)?;
    Ok(FrontMatter::new(dependencies, dev_dependencies, profiles))
}
