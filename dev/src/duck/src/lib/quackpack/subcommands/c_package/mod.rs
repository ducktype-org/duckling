// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! `duck c-package`: generates a Duckling package binding a C library.
//!
//! The package can be published: its recipe (`c-package.yaml`) holds only relative paths and
//! package names, its manifest names libraries without locating them, and its generated bindings
//! record the target they were made for.

mod package;
pub mod recipe;
mod resolve;
mod verify;

use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use serde::Deserialize;

use self::package::{PackagePaths, generated_root_module, write};
use self::recipe::{RECIPE_FILE, Recipe};
use self::resolve::{Resolved, resolve};
use crate::quackpack::core::valid_package_name::validate_package_name;
use crate::util::command_ext::CommandExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Environment variable overriding where `duck_c_import` is found.
const TOOL_ENV: &str = "DUCK_C_IMPORT";
const TOOL_NAME: &str = "duck_c_import";
const DEFAULT_VERSION: &str = "0.1.0";
const LAYOUT_CHECK_FILE: &str = "layout_check.dk";

/// Options of `duck c-package new`.
pub struct NewOptions<'a> {
    pub ctx: &'a DuckContext,
    pub at: PathBuf,
    pub name: Option<&'a str>,
    pub recipe: Recipe,
    pub verify: bool,
}

/// Creates a new package at `options.at`.
pub fn new(options: NewOptions<'_>) -> QuackResult<()> {
    let NewOptions {
        ctx,
        at,
        name,
        mut recipe,
        verify,
    } = options;
    let paths = PackagePaths { root: at };
    for existing in [paths.manifest(), paths.root.join(RECIPE_FILE)] {
        if existing.exists() {
            qp_bail!(
                "`{}` already exists; use `duck c-package regen` to regenerate the package",
                existing.display()
            );
        }
    }
    let name = match name {
        Some(name) => name.to_owned(),
        None => paths
            .root
            .file_name()
            .and_then(|name| name.to_str())
            .map(str::to_owned)
            .context("cannot derive a package name from the path; pass `--name`")?,
    };
    validate_name(&name)?;
    if recipe.headers.is_empty() {
        qp_bail!("no headers given; pass at least one `--header`");
    }
    let cwd = std::env::current_dir().context("failed to get the current directory")?;
    recipe.relativize(&cwd, &paths.root);

    let created_root = !paths.root.exists();
    let result = (|| {
        fs::create_dir_all(&paths.root)
            .with_context(|| format!("failed to create `{}`", paths.root.display()))?;
        let resolved = resolve(&recipe, &paths.root)?;
        let version = resolved
            .versions
            .first()
            .map(|(_, version)| version.clone())
            .filter(|version| is_plain_version(version))
            .unwrap_or_else(|| DEFAULT_VERSION.to_owned());
        let generated = generate(ctx, &paths, &name, &recipe, &resolved)?;
        recipe.write(&paths.root)?;
        package::write_new_manifest(&paths, &name, &version, &resolved)?;
        finish(ctx, &paths, &name, &resolved, generated, verify)
    })();
    if result.is_err() && created_root && !paths.manifest().exists() {
        let _ = fs::remove_dir_all(&paths.root);
    }
    result
}

/// Regenerates the package at `at` from its recipe.
pub fn regen(ctx: &DuckContext, at: PathBuf, verify: bool) -> QuackResult<()> {
    let paths = PackagePaths { root: at };
    let recipe = Recipe::read(&paths.root)?;
    let name = package::read_name(&paths)?;
    let resolved = resolve(&recipe, &paths.root)?;
    let generated = generate(ctx, &paths, &name, &recipe, &resolved)?;
    package::update_manifest_libraries(&paths, &resolved)?;
    finish(ctx, &paths, &name, &resolved, generated, verify)
}

/// Checks that the bindings of the package at `at` are what its recipe generates on this machine,
/// without changing anything.
pub fn check(ctx: &DuckContext, at: PathBuf) -> QuackResult<()> {
    let paths = PackagePaths { root: at };
    let recipe = Recipe::read(&paths.root)?;
    let name = package::read_name(&paths)?;
    let resolved = resolve(&recipe, &paths.root)?;
    let banner = banner(&recipe, &resolved);
    let (_, work) = translate(&paths, &name, &recipe, &resolved, &banner)?;
    let mut expected = read_dir_files(&work.path().join(package::GENERATED_DIR))?;
    expected.remove(LAYOUT_CHECK_FILE);
    let actual = read_dir_files(&paths.generated())?;

    let mut differences = vec![];
    for (file, content) in &expected {
        match actual.get(file) {
            None => differences.push(format!("`{file}` is missing")),
            Some(existing) if existing != content => {
                differences.push(format!("`{file}` differs"))
            }
            Some(_) => {}
        }
    }
    for file in actual.keys().filter(|file| !expected.contains_key(*file)) {
        differences.push(format!("`{file}` is not generated by the recipe"));
    }
    let manifest = ManifestLibraries::read(&paths)?;
    if manifest.external_libs != resolved.external_libs {
        differences.push(format!(
            "`metadata.external-libs` is {:?}, the recipe gives {:?}",
            manifest.external_libs, resolved.external_libs
        ));
    }
    if manifest.external_lib_paths != resolved.external_lib_paths {
        differences.push(format!(
            "`metadata.external-lib-paths` is {:?}, the recipe gives {:?}",
            manifest.external_lib_paths, resolved.external_lib_paths
        ));
    }
    if !differences.is_empty() {
        qp_bail!(
            "the package is out of date with `{RECIPE_FILE}`; run `duck c-package regen`:\n  {}",
            differences.join("\n  ")
        );
    }
    ctx.info(format!("`{name}` is up to date with `{RECIPE_FILE}`"))
}

#[derive(Default, Deserialize)]
#[serde(rename_all = "kebab-case")]
struct ManifestLibraries {
    #[serde(default)]
    external_libs: Vec<String>,
    #[serde(default)]
    external_lib_paths: Vec<String>,
}

impl ManifestLibraries {
    fn read(paths: &PackagePaths) -> QuackResult<Self> {
        #[derive(Deserialize)]
        struct Root {
            metadata: ManifestLibraries,
        }
        let manifest = paths.manifest();
        let text = fs::read_to_string(&manifest)
            .with_context(|| format!("failed to read `{}`", manifest.display()))?;
        let root: Root = serde_yaml_ng::from_str(&text)
            .with_context(|| format!("failed to parse `{}`", manifest.display()))?;
        Ok(root.metadata)
    }
}

fn read_dir_files(dir: &Path) -> QuackResult<BTreeMap<String, String>> {
    let mut files = BTreeMap::new();
    if !dir.exists() {
        return Ok(files);
    }
    for entry in fs::read_dir(dir).with_context(|| format!("failed to read `{}`", dir.display()))? {
        let entry = entry.with_context(|| format!("failed to read `{}`", dir.display()))?;
        let content = fs::read_to_string(entry.path())
            .with_context(|| format!("failed to read `{}`", entry.path().display()))?;
        files.insert(entry.file_name().to_string_lossy().into_owned(), content);
    }
    Ok(files)
}

fn validate_name(name: &str) -> QuackResult<()> {
    validate_package_name(name).with_context(|| format!("`{name}` is not a valid package name"))?;
    if name == package::GENERATED_DIR {
        qp_bail!("`{name}` is the name of a generated module; pick another package name");
    }
    if !name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
        qp_bail!("`{name}` has to be a valid Duckling identifier, since it is the import path");
    }
    Ok(())
}

fn is_plain_version(version: &str) -> bool {
    let parts: Vec<_> = version.split('.').collect();
    parts.len() == 3
        && parts
            .iter()
            .all(|part| !part.is_empty() && part.chars().all(|c| c.is_ascii_digit()))
}

#[derive(Debug, Deserialize)]
struct Report {
    errors: Vec<String>,
    layouts: Vec<String>,
    skipped: Vec<serde_json::Value>,
    counts: serde_json::Map<String, serde_json::Value>,
}

fn banner(recipe: &Recipe, resolved: &Resolved) -> Vec<String> {
    let headers = recipe.headers.join(", ");
    let versions = resolved
        .versions
        .iter()
        .map(|(package, version)| format!("{package} {version}"))
        .collect::<Vec<_>>();
    let from = if versions.is_empty() {
        headers
    } else {
        format!("{headers} ({})", versions.join(", "))
    };
    vec![
        format!("Generated by `duck c-package` from {from}."),
        format!("Do not edit: change `{RECIPE_FILE}` and run `duck c-package regen`."),
    ]
}

fn find_tool() -> PathBuf {
    if let Some(path) = std::env::var_os(TOOL_ENV) {
        return PathBuf::from(path);
    }
    // Installed next to duck, or found on `PATH`.
    if let Some(sibling) = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(|dir| dir.join(TOOL_NAME)))
        .filter(|path| path.is_file())
    {
        return sibling;
    }
    PathBuf::from(TOOL_NAME)
}

/// Runs the translator into a fresh directory, so a failure leaves the previous bindings intact.
/// Relative paths in the recipe are resolved against the package's root.
fn translate(
    paths: &PackagePaths,
    name: &str,
    recipe: &Recipe,
    resolved: &Resolved,
    banner: &[String],
) -> QuackResult<(Report, tempfile::TempDir)> {
    let work = tempfile::tempdir().context("failed to create a temporary directory")?;
    let output_dir = work.path().join(package::GENERATED_DIR);
    let request = serde_json::json!({
        "module_name": name,
        "bindings_import": format!("{name}.{}.{name}", package::GENERATED_DIR),
        "output_dir": output_dir,
        "headers": recipe.headers,
        "clang_args": resolved.cflags,
        "include": recipe.include,
        "exclude": recipe.exclude,
        "banner": banner,
    });
    let request_path = work.path().join("request.json");
    let report_path = work.path().join("report.json");
    write(&request_path, &request.to_string())?;

    let mut command = Command::new(find_tool());
    command.arg(&request_path).arg(&report_path);
    command.current_dir(&paths.root);
    let output = command.output().with_context(|| {
        format!(
            "failed to run `{}`; it is built next to duckc, or can be pointed at with ${TOOL_ENV}",
            command.display()
        )
    })?;
    let report_text = fs::read_to_string(&report_path).with_context(|| {
        format!(
            "`{TOOL_NAME}` produced no report: {}",
            String::from_utf8_lossy(&output.stderr).trim()
        )
    })?;
    let report: Report = serde_json::from_str(&report_text)
        .with_context(|| format!("`{TOOL_NAME}` produced an invalid report"))?;
    if !report.errors.is_empty() {
        let shown = report.errors.iter().take(20).cloned().collect::<Vec<_>>();
        qp_bail!(
            "the headers do not compile:\n{}{}",
            shown.join("\n"),
            if report.errors.len() > shown.len() {
                format!("\n... and {} more", report.errors.len() - shown.len())
            } else {
                String::new()
            }
        );
    }
    write(
        &output_dir.join(format!("{}.dk", package::GENERATED_DIR)),
        &generated_root_module(banner),
    )?;
    Ok((report, work))
}

/// What [`generate`] produced, for [`finish`].
struct Generated {
    report: Report,
    layout_check: String,
}

/// Translates the headers and replaces `src/generated/` with the result.
fn generate(
    ctx: &DuckContext,
    paths: &PackagePaths,
    name: &str,
    recipe: &Recipe,
    resolved: &Resolved,
) -> QuackResult<Generated> {
    let banner = banner(recipe, resolved);
    let (report, work) = translate(paths, name, recipe, resolved, &banner)?;
    let staged = work.path().join(package::GENERATED_DIR);
    // The layout check is specific to the target, so it is not published with the package.
    let layout_check_path = staged.join(LAYOUT_CHECK_FILE);
    let layout_check = fs::read_to_string(&layout_check_path)
        .with_context(|| format!("`{TOOL_NAME}` did not write `{LAYOUT_CHECK_FILE}`"))?;
    fs::remove_file(&layout_check_path)
        .with_context(|| format!("failed to remove `{}`", layout_check_path.display()))?;

    let generated = paths.generated();
    if generated.exists() {
        fs::remove_dir_all(&generated)
            .with_context(|| format!("failed to remove `{}`", generated.display()))?;
    }
    fs::create_dir_all(&generated)
        .with_context(|| format!("failed to create `{}`", generated.display()))?;
    for (file, content) in read_dir_files(&staged)? {
        write(&generated.join(file), &content)?;
    }
    package::write_root_module_if_missing(paths, name)?;

    let count = |key: &str| report.counts.get(key).and_then(|v| v.as_u64()).unwrap_or(0);
    ctx.info(format!(
        "generated `{name}`: {} classes, {} functions, {} constants",
        count("classes"),
        count("functions"),
        count("constants")
    ))?;
    if !report.skipped.is_empty() {
        ctx.info(format!(
            "skipped {} declarations; the reasons are listed at the top of `{}`",
            report.skipped.len(),
            generated.join(format!("{name}.dk")).display()
        ))?;
    }
    for warning in &resolved.warnings {
        ctx.warning(warning)?;
    }
    Ok(Generated {
        report,
        layout_check,
    })
}

fn finish(
    ctx: &DuckContext,
    paths: &PackagePaths,
    name: &str,
    resolved: &Resolved,
    generated: Generated,
    verify: bool,
) -> QuackResult<()> {
    if !verify {
        return Ok(());
    }
    // @TODO: #4144 check on the DVM too once `external-libs` work there.
    let dvm = resolved.external_libs.is_empty();
    if !dvm {
        ctx.info("not checking on the DVM, which does not load `external-libs` yet")?;
    }
    verify::verify(
        ctx,
        &paths.root,
        name,
        &generated.layout_check,
        &generated.report.layouts,
        &verify::Backends { native: true, dvm },
    )?;
    ctx.info("the package builds, links and matches the C layouts")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn plain_versions() {
        assert!(is_plain_version("3.4.16"));
        assert!(!is_plain_version("3.4"));
        assert!(!is_plain_version("1.2.3-beta"));
    }

    #[test]
    fn names_must_be_identifiers() {
        assert!(validate_name("sdl3").is_ok());
        assert!(validate_name("my-lib").is_err());
        assert!(validate_name("generated").is_err());
    }
}
