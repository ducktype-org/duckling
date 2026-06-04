//! Parsing of the frontmatter from its schema.
//! A frontmatter is a fragment of yaml code similiar to a manifest, at the beginning of a Duckling script.
//! It can specify script's dependencies, allowing the script to be run without any venv.
//! Only fields allowed inside the frontmatter are `dependencies`, `dev-dependencies` and `profiles`.
//! A frontmatter can also be a singular `import` field, with a path to a yaml file, which contains the real frontmatter contents.
//! If the path is relative it is resolved from the folder in which the script is.
//!
//! # Example
//! ```text
//! <frontmatter>
//! dependencies:
//!   xd:
//!     version: '2.0.0'
//! dev-depedencies:
//!   xdd:
//!     version: '3.4.5'
//! profiles:
//!   my-profile:
//!     opt-level: O3
//! </frontmatter>
//!
//! builtin_output_i64(0)
//! ```
use std::path::{Path, PathBuf};
use std::sync::LazyLock;

use regex::Regex;
use serde::Deserialize;
use tracing::debug;

use crate::quackpack::core::FrontMatterScript;
use crate::quackpack::core::manifest::parse::manifest::{ParseMode, parse};
use crate::{
    DuckContext, QuackResult, qp_bail, quackpack::schemas::manifest::Manifest as ManifestSchema,
    util::path_ops_ext::PathOpsExt,
};
use crate::{QuackResultContext, qp_err};

pub static FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\s*<frontmatter>([\s\S]*)</frontmatter>").unwrap());
pub static UNCLOSED_FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\s*<frontmatter>").unwrap());

/// Parse a frontmatter of a script at a given `path`.
/// If the script does not contain a frontmatter, returns `Ok(None)`.
#[cfg_attr(not(test), expect(dead_code))]
pub fn try_parse_frontmatter(
    path: PathBuf,
    ctx: &DuckContext,
) -> QuackResult<Option<FrontMatterScript>> {
    debug!("starting parsing...");
    try_parse_inner(path.clone(), ctx).with_context(|| {
        format!(
            "when trying to parse the frontmatter of the script at `{}`",
            path.display()
        )
    })
}

/// Helper for [`_try_parse_frontmatter`].
/// First generates the appropriate [`ManifestSchema`] and then parses it into [`FrontMatterScript`].
/// If the script does not contain a frontmatter, returns `Ok(None)`.
fn try_parse_inner(path: PathBuf, ctx: &DuckContext) -> QuackResult<Option<FrontMatterScript>> {
    let Some(schema) = generate_schema(&path)? else {
        return Ok(None);
    };
    let frontmatter = parse(&schema, &path, ParseMode::FrontMatterScript, ctx)?;
    Ok(Some(FrontMatterScript::new(path, schema, frontmatter)))
}

/// Helper for [`_try_parse_frontmatter`].
/// Generate a [`ManifestSchema`] from the script's contents.
/// This includes resolving import, meaning that if the frontmatter has the `import` field,
/// the schema is generated based on the path specified in the import.
/// If the script does not contain a frontmatter, returns `Ok(None)`.
fn generate_schema(path: &Path) -> QuackResult<Option<ManifestSchema>> {
    let content = path.read_to_string()?;
    // @TODO: #2860 Finalize frontmatters syntax
    // This has a bug when `</frontmatter>` is in a yaml comment (maybe don't care / make it a feature).
    // Moreover the syntax is not yet finalized.
    let Some(captures) = FRONTMATTER_REGEX.captures(&content) else {
        if UNCLOSED_FRONTMATTER_REGEX.captures(&content).is_some() {
            qp_bail!("frontmatter begins but does not end")
        }
        return Ok(None);
    };
    let frontmatter_content = captures.get(1).unwrap().as_str();
    Some(generate_schema_from_content(
        path,
        frontmatter_content,
        true,
    ))
    .transpose()
}

/// Helper for [`_generate_schema`].
/// Given the raw string generates the corresponding [`ManifestSchema`].
/// With `resolve_imports` set to true, resolves the potential import,
/// otherwise checks that there is no import.
fn generate_schema_from_content(
    path: &Path,
    content: &str,
    resolve_imports: bool,
) -> QuackResult<ManifestSchema> {
    let deserializer = serde_yaml_ng::Deserializer::from_str(content);
    let schema = ManifestSchema::deserialize(deserializer)?;
    if resolve_imports {
        resolve_import_in_schema(path, schema)
    } else if schema.import.is_some() {
        qp_bail!("imported frontmatter cannot have `import` field itself")
    } else {
        Ok(schema)
    }
}

/// Helper for [`_generate_schema_from_content`].
/// Given a [`ManifestSchema`], if it contains an import,
/// checks that other fields are empty and generates a schema from the fiel specified in the import.
/// Import path is treated as relative to the folder where the importing script is contained,
/// but absolute paths work as well.
fn resolve_import_in_schema(
    script_path: &Path,
    schema: ManifestSchema,
) -> QuackResult<ManifestSchema> {
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
        let content = imported_path.read_to_string().with_context(|| {
            format!(
                "while reading the file imported by `{}` at `{}`",
                script_path.display(),
                imported_path.display(),
            )
        })?;
        let imported_schema = generate_schema_from_content(&imported_path, &content, false)
            .with_context(|| {
                format!(
                    "while generating schema from the file imported by `{}` at `{}`",
                    script_path.display(),
                    imported_path.display(),
                )
            })?;
        return Ok(imported_schema);
    }
    Ok(schema)
}
