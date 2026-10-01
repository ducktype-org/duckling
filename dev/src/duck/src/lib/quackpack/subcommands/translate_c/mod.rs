//! `duck translate-c`: generates a Duckling package binding a C library installed on this
//! machine.
//!
//! The package is a local build artifact, specific to the machine: its manifest records the
//! pkg-config output, library paths and sonames found here, and its bindings the layouts of the
//! headers installed here. It is regenerated from the `c-bindings` recipe in its manifest rather
//! than committed or shared.

mod elf;
mod package;
pub mod recipe;
mod resolve;
mod verify;

use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use serde::Deserialize;

use self::package::{PackagePaths, generated_root_module, write};
use self::recipe::CBindingsRecipe;
use self::resolve::{Resolved, resolve};
use crate::quackpack::core::valid_package_name::validate_package_name;
use crate::util::command_ext::CommandExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Environment variable overriding where `duck_c_import` is found.
const TOOL_ENV: &str = "DUCK_C_IMPORT";
const TOOL_NAME: &str = "duck_c_import";
const DEFAULT_VERSION: &str = "0.1.0";

/// Options of `duck translate-c new`.
pub struct NewOptions<'a> {
    pub ctx: &'a DuckContext,
    pub at: PathBuf,
    pub name: Option<&'a str>,
    pub recipe: CBindingsRecipe,
    pub verify: bool,
}

/// Options of `duck translate-c regen`.
pub struct RegenOptions<'a> {
    pub ctx: &'a DuckContext,
    pub at: PathBuf,
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
    if paths.manifest().exists() {
        qp_bail!(
            "`{}` already exists; use `duck translate-c regen` to regenerate it",
            paths.manifest().display()
        );
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

    // Paths are made absolute now, so a regen from any directory reads the same files.
    let cwd = std::env::current_dir().context("failed to get the current directory")?;
    for header in &mut recipe.headers {
        let path = cwd.join(&*header);
        if path.is_file() {
            *header = path.display().to_string();
        }
    }
    for library in &mut recipe.libraries {
        if !library.starts_with('-') {
            *library = cwd.join(&*library).display().to_string();
        }
    }

    let resolved = resolve(&recipe, &cwd)?;
    let version = resolved
        .versions
        .first()
        .map(|(_, version)| version.clone())
        .filter(|version| is_plain_version(version))
        .unwrap_or_else(|| DEFAULT_VERSION.to_owned());
    generate(ctx, &paths, &name, &version, &recipe, &resolved, verify)
}

/// Regenerates the package at `options.at` from its recipe.
pub fn regen(options: RegenOptions<'_>) -> QuackResult<()> {
    let paths = PackagePaths { root: options.at };
    let existing = package::read_existing(&paths)?;
    let resolved = resolve(&existing.recipe, &paths.root)?;
    let version = resolved
        .versions
        .first()
        .map(|(_, version)| version.clone())
        .filter(|version| is_plain_version(version))
        .unwrap_or(existing.version);
    generate(
        options.ctx,
        &paths,
        &existing.name,
        &version,
        &existing.recipe,
        &resolved,
        options.verify,
    )
}

fn validate_name(name: &str) -> QuackResult<()> {
    validate_package_name(name).with_context(|| format!("`{name}` is not a valid package name"))?;
    if name == package::GENERATED_DIR || name == "layout_check" {
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

fn banner(recipe: &CBindingsRecipe, resolved: &Resolved) -> Vec<String> {
    let headers = recipe
        .headers
        .iter()
        .map(|header| {
            Path::new(header)
                .file_name()
                .filter(|_| Path::new(header).is_absolute())
                .map_or_else(
                    || header.clone(),
                    |name| name.to_string_lossy().into_owned(),
                )
        })
        .collect::<Vec<_>>()
        .join(", ");
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
        format!("Generated by `duck translate-c` from {from}."),
        "Do not edit: change `c-bindings` in quackconfig.yaml and run `duck translate-c regen`."
            .to_owned(),
        "Specific to the machine it was generated on; regenerate it instead of committing it."
            .to_owned(),
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
fn translate(
    paths: &PackagePaths,
    name: &str,
    recipe: &CBindingsRecipe,
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
    command.current_dir(paths.root.parent().unwrap_or(Path::new(".")));
    let output = command.output().with_context(|| {
        format!(
            "failed to run `{}`; it is built next to duckc when libclang is available, or can be \
             pointed at with ${TOOL_ENV}",
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
    Ok((report, work))
}

fn generate(
    ctx: &DuckContext,
    paths: &PackagePaths,
    name: &str,
    version: &str,
    recipe: &CBindingsRecipe,
    resolved: &Resolved,
    verify: bool,
) -> QuackResult<()> {
    let banner = banner(recipe, resolved);
    let (report, work) = translate(paths, name, recipe, resolved, &banner)?;

    let generated = paths.generated();
    if generated.exists() {
        fs::remove_dir_all(&generated)
            .with_context(|| format!("failed to remove `{}`", generated.display()))?;
    }
    fs::create_dir_all(paths.src()).context("failed to create the package's `src`")?;
    let staged = work.path().join(package::GENERATED_DIR);
    // A rename cannot cross file systems, so fall back to copying.
    if fs::rename(&staged, &generated).is_err() {
        fs::create_dir_all(&generated)
            .with_context(|| format!("failed to create `{}`", generated.display()))?;
        for entry in fs::read_dir(&staged).context("failed to read the generated files")? {
            let entry = entry.context("failed to read the generated files")?;
            fs::copy(entry.path(), generated.join(entry.file_name()))
                .context("failed to copy the generated files")?;
        }
    }
    write(
        &generated.join(format!("{}.dk", package::GENERATED_DIR)),
        &generated_root_module(&banner),
    )?;
    package::write_root_module_if_missing(paths, name)?;
    package::write_manifest(paths, name, version, recipe, resolved)?;

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
    for warning in &resolved.dvm_warnings {
        ctx.warning(warning)?;
    }

    if verify {
        verify::verify(
            ctx,
            &paths.root,
            name,
            &report.layouts,
            &verify::Backends {
                native: true,
                dvm: !resolved.dvm_shared_libs.is_empty(),
            },
        )?;
        ctx.info("the package builds, links and matches the C layouts")?;
    }
    Ok(())
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
    }
}
