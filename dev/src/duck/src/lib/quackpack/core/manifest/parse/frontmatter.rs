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
use std::path::Path;
use std::sync::LazyLock;

use regex::{Captures, Regex};
use tracing::{debug, trace};

use super::parse_schema;
use crate::quackpack::core::ParseMode;
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::manifest::parse::manifest::parse;
use crate::quackpack::core::script::FrontMatter;
use crate::quackpack::schemas::manifest::Manifest as ManifestSchema;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail, qp_err};

pub static FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\s*<frontmatter>([\s\S]*)</frontmatter>").unwrap());
pub static UNCLOSED_FRONTMATTER_REGEX: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^\s*<frontmatter>").unwrap());

/// Parse a frontmatter of a script at a given `path`.
///
/// Script doesn't have to have a frontmatter; in that case, a default will be returned.
pub fn parse_frontmatter(path: &Path, ctx: &DuckContext) -> QuackResult<(FrontMatter, Warnings)> {
    trace!("starting parsing");
    parse_inner(path, ctx).with_context(|| {
        format!(
            "when trying to parse the frontmatter of the script at `{}`",
            path.display()
        )
    })
}

/// Helper for [`parse_frontmatter`].
/// First generates the appropriate [`ManifestSchema`] and then parses it into [`FrontMatter`].
fn parse_inner(path: &Path, ctx: &DuckContext) -> QuackResult<(FrontMatter, Warnings)> {
    let mut warnings = Warnings::default();
    let schema = generate_schema(path, &mut warnings)?;
    let frontmatter = parse(&schema, path, ParseMode::FrontMatter, &mut warnings, ctx)?;
    FrontMatter::new(path.to_path_buf(), schema, Box::new(frontmatter))
        .map(|frontmatter| (frontmatter, warnings))
}

/// Try to capture a frontmatter from the given contents.
pub fn capture_frontmatter(contents: &str) -> QuackResult<Option<Captures<'_>>> {
    let Some(captures) = FRONTMATTER_REGEX.captures(contents) else {
        debug!("no frontmatter");
        if UNCLOSED_FRONTMATTER_REGEX.captures(contents).is_some() {
            qp_bail!("frontmatter begins but does not end")
        }
        return Ok(None);
    };
    debug!("has frontmatter");
    Ok(Some(captures))
}

/// Helper for [`parse_frontmatter`].
/// Generate a [`ManifestSchema`] from the script's contents.
/// This includes resolving import, meaning that if the frontmatter has the `import` field,
/// the schema is generated based on the path specified in the import.
/// If the script does not contain a frontmatter, returns [`ManifestSchema`] with `None`s.
fn generate_schema(path: &Path, warnings: &mut Warnings) -> QuackResult<ManifestSchema> {
    let content = path.read_to_string()?;
    // @TODO: #2860 Finalize frontmatters syntax
    // This has a bug when `</frontmatter>` is in a yaml comment (maybe don't care / make it a feature).
    // Moreover the syntax is not yet finalized.
    let Some(captures) = capture_frontmatter(&content)? else {
        return Ok(ManifestSchema {
            metadata: None,
            dependencies: None,
            dev_dependencies: None,
            features: None,
            profiles: None,
            import: None,
            venv: None,
        });
    };
    let frontmatter_content = captures.get(1).unwrap().as_str();
    generate_schema_from_content(
        path,
        frontmatter_content,
        /* resolve_imports */ true,
        warnings,
    )
}

/// Helper for [`generate_schema`].
/// Given the raw string generates the corresponding [`ManifestSchema`].
/// With `resolve_imports` set to true, resolves the potential import,
/// otherwise checks that there is no import.
fn generate_schema_from_content(
    path: &Path,
    content: &str,
    resolve_imports: bool,
    warnings: &mut Warnings,
) -> QuackResult<ManifestSchema> {
    let schema = parse_schema(content, warnings)?;
    if resolve_imports {
        resolve_import_in_schema(path, schema, warnings)
    } else if schema.import.is_some() {
        qp_bail!("imported frontmatter cannot have `import` field itself")
    } else {
        Ok(schema)
    }
}

/// Helper for [`generate_schema_from_content`].
/// Given a [`ManifestSchema`], if it contains an import,
/// checks that other fields are empty and generates a schema from the file specified in the import.
/// Import path is treated as relative to the folder where the importing script is contained,
/// but absolute paths work as well.
fn resolve_import_in_schema(
    script_path: &Path,
    schema: ManifestSchema,
    warnings: &mut Warnings,
) -> QuackResult<ManifestSchema> {
    if let Some(ref import) = schema.import {
        debug!(
            script = %script_path.display(),
            import = %import.display(),
            "imports a frontmatter",
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
        let imported_schema = generate_schema_from_content(
            &imported_path,
            &content,
            /* resolve_imports */ false,
            warnings,
        )
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
