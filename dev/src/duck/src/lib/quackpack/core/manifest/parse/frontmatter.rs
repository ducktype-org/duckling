use std::path::{Path, PathBuf};
use std::sync::LazyLock;

use regex::Regex;
use serde::Deserialize;
use tracing::debug;

use crate::QuackResultContext;
use crate::quackpack::core::FrontMatterScript;
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
    LazyLock::new(|| Regex::new(r"^\s*<frontmatter>([\s\S]*)</frontmatter>").unwrap());
pub static UNCLOSED_FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\s+<frontmatter>").unwrap());

/// Parse a frontmatter of a script at a given `path`.
pub fn parse_frontmatter(
    path: PathBuf,
    ctx: &DuckContext,
) -> QuackResult<Option<FrontMatterScript>> {
    debug!("starting parsing...");
    parse_inner(path.clone(), ctx).with_context(|| {
        format!(
            "when trying to parse the frontmatter of the script at `{}`",
            path.display()
        )
    })
}

/// Helper for [`parse_frontmatter`].
fn parse_inner(path: PathBuf, ctx: &DuckContext) -> QuackResult<Option<FrontMatterScript>> {
    if let Some((frontmatter, schema)) = parse(&path, ctx)? {
        Ok(Some(FrontMatterScript::new(path, schema, frontmatter)))
    } else {
        Ok(None)
    }
}

/// Deserialize a script frontmatter.
#[tracing::instrument(skip(ctx))]
fn parse(path: &Path, ctx: &DuckContext) -> QuackResult<Option<(FrontMatter, FrontMatterSchema)>> {
    let Some(schema) = generate_schema_script(path)? else {
        return Ok(None);
    };
    let frontmatter = parse_schema_script(path, &schema, ctx)?;
    Ok(Some((frontmatter, schema)))
}

/// Generate a [`FrontMatterSchema`] from the script's contents.
fn generate_schema_script(path: &Path) -> QuackResult<Option<FrontMatterSchema>> {
    let content = path.read_to_string()?;
    println!("{content}");
    let Some(captures) = FRONTMATTER_REGEX.captures(&content) else {
        if UNCLOSED_FRONTMATTER_REGEX.captures(&content).is_some() {
            qp_bail!("frontmatter begins but does not end")
        }
        return Ok(None);
    };
    let frontmatter_content = captures.get(1).unwrap().as_str();
    Some(generate_schema(frontmatter_content)).transpose()
}

/// Helper for [`parse_schema_script`].
/// Generate [`FrontMatterSchema`] from a yaml file, which was imported by another frontmatter.
fn generate_schema_imported(path: &Path) -> QuackResult<FrontMatterSchema> {
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
            "script at `{}` imports frontmatter at `{}`",
            script_path.display(),
            import.display()
        );
        if !schema.is_just_import_or_empty() {
            let mut err = qp_err!(
                "script at `{}` imports a frontmatter but also specifies some of the frontmatter fields",
                script_path.display()
            );
            err = err.add_hint("either remove the `import` field or all the other fields");
            qp_bail!(err);
        }
        let root_path = script_path
            .parent()
            .expect("Script path should point to a file with parent");
        let imported_path = root_path.join(import);
        let imported_schema = generate_schema_imported(&imported_path).with_context(|| {
            format!(
                "while reading the frontmatter imported by `{}` at `{}`",
                script_path.display(),
                imported_path.display(),
            )
        })?;
        return parse_schema_imported(import, &imported_schema, ctx).with_context(|| {
            format!(
                "while parsing the frontmatter imported by `{}` at `{}`",
                script_path.display(),
                imported_path.display(),
            )
        });
    }
    parse_schema(script_path, schema, ctx)
}

/// Helper for [`parse_schema_script`].
/// Generate [`FrontMatter`] from [`FrontMatterSchema`] of an imported yaml file.
#[tracing::instrument(skip(schema, ctx))]
fn parse_schema_imported(
    yaml_file_path: &Path,
    schema: &FrontMatterSchema,
    ctx: &DuckContext,
) -> QuackResult<FrontMatter> {
    if schema.import.is_some() {
        qp_bail!(
            "Transitive import at `{}`, which is prohibited",
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
