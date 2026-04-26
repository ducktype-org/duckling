//! Initialize a new project.
use std::io::Write;
use std::path::{Path, PathBuf};

use git2::{Repository, RepositoryInitOptions};
use regex::Regex;

use crate::quackpack::core::{PackageLoader, VenvConfig, Version};
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::user_prompts;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail};

/// Options for initializing a new project.
pub struct InitOptions<'duck> {
    pub ctx: &'duck DuckContext,
    /// Root of the project.
    pub at: PathBuf,
    /// Name of the project.
    pub name: StrId,
    /// Initialize a venv instead of a project (do not create the `src` folder).
    pub as_venv: bool,
    /// Make the project expose freezefile.
    pub expose_freezefile: bool,
    /// Mark the project as ephemeral.
    pub ephemeral: bool,
    /// Make the project contain a storage and use it.
    pub local_storage: bool,
    /// Initialize the project as a git repository.
    pub git: bool,
    /// Use prompts to customize the manifest.
    pub full: bool,
}

const DEFAULT_SOURCE_FILENAME: &str = "src.dmf";

const DEFAULT_SOURCE_CONTENTS: &str = "\
fun main() = {
    # !TODO: On macOS, builtin_output_string segfaults :^);
    # builtin_output_string(\"Hello, world!\");
    return 0;
}
";

const DEFAULT_GITIGNORE: &str = "\
.duck_build
";

/// Initialize a new project with the given options.
pub fn init(opts: InitOptions<'_>) -> QuackResult<()> {
    bail_if_would_override_project(opts.ctx, &opts.at)?;
    create_manifest_file(&opts.at, opts.name, opts.full)?;
    create_venv_config_file(
        opts.ctx,
        &opts.at,
        opts.expose_freezefile,
        opts.ephemeral,
        opts.local_storage,
    )?;
    if !opts.as_venv {
        add_package_structure(&opts.at)?;
    }
    if opts.git {
        init_git(&opts.at)?;
    }
    opts.ctx.console().info(format!(
        "successfully created a new project `{}` at `{}`",
        opts.name,
        opts.at.display()
    ));
    Ok(())
}

/// Error, if we were to override an existing project.
fn bail_if_would_override_project(ctx: &DuckContext, root: &Path) -> QuackResult<()> {
    if let Ok(package) = PackageLoader::find_at_exact_directory(root, ctx) {
        qp_bail!(
            "cannot reinitialize project `{}` at `{}`",
            package.package().manifest().name(),
            package.package().root_directory().display()
        )
    }
    Ok(())
}

/// Create a file with the manifest of the project.
fn create_manifest_file(root_path: &Path, name: StrId, full: bool) -> QuackResult<()> {
    let manifest_file = root_path.join(PackageLoader::MANIFEST_NAME);
    manifest_file
        .touch()
        .context("failed to create a manifest file")?;
    let manifest_contents = if full {
        manifest_with_user_prompts(name)?
    } else {
        make_default_manifest_for_name(&name)
    };
    manifest_file
        .write(manifest_contents)
        .context("failed to write a default manifest")?;
    Ok(())
}

/// Create a default manifest.
fn make_default_manifest_for_name(name: &str) -> String {
    format!(
        "\
metadata:
  name: {name}
  version: '1.0.0'
"
    )
}

/// Create custom manifest from user prompts.
fn manifest_with_user_prompts(name: StrId) -> QuackResult<String> {
    let authors_regex = Regex::new(r"^\[.*\]$").unwrap();
    let name = user_prompts::string_with_default("Enter the project's name".into(), name)?;
    let authors = user_prompts::string_no_default_with_regex(
        "Enter the project's authors in a list e.g. `[<author1>, <author2>]`".into(),
        authors_regex,
    )?;
    let version = user_prompts::with_default(
        "Enter the version of the project".into(),
        Version::default(),
    )?;
    Ok(format!(
        "\
metadata:
  name: {name}
  version: '{version}'
  authors: {authors}
"
    ))
}

/// Create a file with the venv config of the project (if necessary).
fn create_venv_config_file(
    ctx: &DuckContext,
    root_path: &Path,
    expose_freezefile: bool,
    ephemeral: bool,
    local_storage: bool,
) -> QuackResult<()> {
    let venv_cfg_file = root_path.join(PackageLoader::VENV_CONFIG_NAME);
    if let Some(venv_cfg) = generate_venv_config(expose_freezefile, ephemeral, local_storage)
        .context_internal("failed to generate a VenvConfig")?
    {
        if venv_cfg_file.exists() {
            ctx.error_console().warning(format!("init run with non-default venv configuration flags, but venv configuration file already exists at `{}`", venv_cfg_file.display()));
            return Ok(());
        }
        let mut venv_cfg_file = venv_cfg_file
            .touch()
            .context("failed to create a venv configuration file")?;
        venv_cfg_file
            .write(venv_cfg.to_string().as_bytes())
            .context("failed to write to a venv configuration file")?;
    }
    Ok(())
}

/// Create a [`VenvConfig`], from the options passed to [`init`].
/// If the generated [`VenvConfig`] has only default values, an [`Option::None`] is returned instead.
fn generate_venv_config(
    expose_freezefile: bool,
    ephemeral: bool,
    local_storage: bool,
) -> QuackResult<Option<VenvConfig>> {
    let mut venv_cfg = VenvConfig::default();
    if expose_freezefile {
        venv_cfg.set_freezefile_exposed(true)?;
    }
    if ephemeral {
        venv_cfg.set_ephemeral(true)?;
    }
    if local_storage {
        venv_cfg.set_storage_path(Path::new("storage"))?;
    }
    let would_create_not_default_venv_config = expose_freezefile || ephemeral || local_storage;
    if would_create_not_default_venv_config {
        Ok(Some(venv_cfg))
    } else {
        Ok(None)
    }
}

/// Add a package structure to the project.
/// Note:
/// -----
/// Currently makes the project's main entry point a `.dmf` file with a `main()` function.
/// In the future an option should be added to initialize the project's entry point as a script (`main.ds`).
fn add_package_structure(root_path: &Path) -> QuackResult<()> {
    let source_file = root_path.join("src").join(DEFAULT_SOURCE_FILENAME);
    if !source_file.exists() {
        source_file
            .touch()
            .context("failed to create a default source file")?;
        source_file
            .write(DEFAULT_SOURCE_CONTENTS)
            .context("failed to write a default duck file")?;
    }
    Ok(())
}

/// Initialize git repository in the project and add `gitignore`.
fn init_git(root_path: &Path) -> QuackResult<()> {
    let mut init_opts = RepositoryInitOptions::new();
    Repository::init_opts(root_path, init_opts.no_reinit(true))
        .context("failed to initialize a git repository")?;
    let gitignore_file = root_path.join(".gitignore");
    gitignore_file
        .touch()
        .context("failed to create a `.gitignore` file")?;
    gitignore_file
        .write(DEFAULT_GITIGNORE)
        .context("failed to write to a `.gitignore` file")?;
    Ok(())
}
