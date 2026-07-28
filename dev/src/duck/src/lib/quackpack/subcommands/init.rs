//! Initialize a new project.
use std::fmt::Display;
use std::fs::{File, OpenOptions};
use std::io::{ErrorKind, Write};
use std::path::{Path, PathBuf};

use git2::Repository;

use crate::duck::util::terminal::Terminal;
use crate::quackpack::core::{PackageLoader, Version};
use crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_err};

/// Options for initializing a new project.
pub struct InitOptions<'duck, 'a> {
    pub ctx: &'duck DuckContext,
    /// Root of the project.
    pub at: PathBuf,
    /// Name of the project.
    pub explicit_name: Option<&'a str>,
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

#[derive(Default)]
/// A helper for building a partial [`VenvConfig`](crate::quackpack::core::manifest::VenvConfig).
struct VenvConfigBuilder {
    expose_freezefile: Option<bool>,
    ephemeral: Option<bool>,
    storage: Option<PathBuf>,
}

impl Display for VenvConfigBuilder {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "venv:")?;

        macro_rules! write_field {
            ($name:expr, $field:expr) => {{
                if let Some(field) = $field {
                    writeln!(f, "  {}: {}", $name, field)?;
                }
            }};
        }
        write_field!("expose-freezefile", self.expose_freezefile);
        write_field!("ephemeral", self.ephemeral);
        write_field!("storage-path", self.storage.as_ref().map(|x| x.display()));
        Ok(())
    }
}

const DEFAULT_SOURCE_FILENAME: &str = "src.dmf";

const DEFAULT_SOURCE_CONTENTS: &str = "\
import core.builtins.*;

fun main() = {
    return 0;
}
";

const DEFAULT_GITIGNORE: &str = "\
.duck_build
";

/// Initialize a new project with the given options.
pub fn init(opts: InitOptions<'_, '_>) -> QuackResult<()> {
    let name = match opts.explicit_name {
        Some(explicit) => explicit.to_owned(),
        None => opts
            .at
            .file_name()
            .with_context(|| format!("path `{}` doesn't have a filename", opts.at.display()))?
            .to_string_lossy()
            .into_owned(),
    };
    create_manifest_file(opts.ctx, &opts.at, &name, opts.full)?;
    append_venv_config_to_manifest(
        &opts.at,
        opts.expose_freezefile,
        opts.ephemeral,
        opts.local_storage,
    )?;
    if !opts.as_venv {
        add_package_structure(opts.ctx, &opts.at)?;
    }
    if opts.git {
        init_git(opts.ctx, &opts.at)?;
    }
    opts.ctx.console().info(format!(
        "successfully created a new project `{}` at `{}`",
        name,
        opts.at.display()
    ))?;
    Ok(())
}

/// Create a file with the manifest of the project.
fn create_manifest_file(
    ctx: &DuckContext,
    root_path: &Path,
    name: &str,
    full: bool,
) -> QuackResult<()> {
    let manifest_path = root_path.join(PackageLoader::MANIFEST_NAME);
    let manifest_contents = if full {
        manifest_with_user_prompts(ctx.console(), name)?
    } else {
        make_default_manifest_for_name(name)
    };
    if let Some(parent) = manifest_path.parent() {
        parent.mkdir(MkdirOptions::WithParents)?;
    }
    let mut manifest_file = match File::create_new(manifest_path) {
        Err(err) => {
            if matches!(err.kind(), ErrorKind::AlreadyExists) {
                qp_bail!(bail_on_overriding_project(ctx, root_path));
            } else {
                return Err(err).context("failed to create the manifest file");
            }
        }
        Ok(manifest_file) => manifest_file,
    };
    manifest_file
        .write_all(manifest_contents.as_bytes())
        .context("failed to write to the manifest file")?;
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
fn manifest_with_user_prompts(terminal: &Terminal, name: &str) -> QuackResult<String> {
    if terminal.verbosity().is_quiet() {
        qp_bail!("cannot create manifest from user input on quiet verbosity");
    }
    let name = terminal.prompt_once_with_default("Enter the project's name", name.to_owned())?;
    let author = terminal.prompt_once("Enter the project's author")?;
    let version = terminal
        .prompt_until_valid_with_default("Enter the version of the project", Version::default());
    Ok(format!(
        "\
metadata:
  name: {name}
  version: '{version}'
  authors: ['{author}']
"
    ))
}

/// Error to return if a project already exists at the location.
fn bail_on_overriding_project(ctx: &DuckContext, root: &Path) -> QuackError {
    let Ok(package) = PackageLoader::find_at_exact_directory(root, ctx) else {
        return qp_err!(
            "There is already a manifest file at `{}`",
            root.join(PackageLoader::MANIFEST_NAME).display()
        );
    };
    qp_err!(
        "cannot reinitialize project `{}` at `{}`",
        package.package().name(),
        package.package().root().display()
    )
}

/// Create a file with the venv config of the project (if necessary).
fn append_venv_config_to_manifest(
    root_path: &Path,
    expose_freezefile: bool,
    ephemeral: bool,
    local_storage: bool,
) -> QuackResult<()> {
    let manifest_file = root_path.join(PackageLoader::MANIFEST_NAME);
    let Some(venv_cfg) = generate_venv_config(expose_freezefile, ephemeral, local_storage) else {
        return Ok(());
    };
    if let Some(parent) = manifest_file.parent() {
        parent.mkdir(MkdirOptions::WithParents)?;
    }
    let mut file = OpenOptions::new()
        .append(true)
        .open(manifest_file)
        .context("failed to open `quackconfig.yaml` for appending")?;
    file.write_all(venv_cfg.to_string().as_bytes())
        .context("failed to append a venv configuration into `quackconfig.yaml`")?;
    Ok(())
}

/// Create a [`VenvConfig`], from the options passed to [`init`].
/// If the generated [`VenvConfig`] has only default values, an [`Option::None`] is returned instead.
fn generate_venv_config(
    expose_freezefile: bool,
    ephemeral: bool,
    local_storage: bool,
) -> Option<VenvConfigBuilder> {
    let mut venv_cfg = VenvConfigBuilder::default();
    if expose_freezefile {
        venv_cfg.expose_freezefile = Some(true);
    }
    if ephemeral {
        venv_cfg.ephemeral = Some(true);
    }
    if local_storage {
        venv_cfg.storage = Some(PathBuf::from("storage"));
    }
    let would_create_not_default_venv_config = expose_freezefile || ephemeral || local_storage;
    if would_create_not_default_venv_config {
        Some(venv_cfg)
    } else {
        None
    }
}

/// Add a package structure to the project.
/// Note:
/// -----
/// Currently makes the project's main entry point a `.dmf` file with a `main()` function.
/// In the future an option should be added to initialize the project's entry point as a script (`main.ds`).
fn add_package_structure(ctx: &DuckContext, root_path: &Path) -> QuackResult<()> {
    let source_file_path = root_path.join("src").join(DEFAULT_SOURCE_FILENAME);
    if let Some(parent) = source_file_path.parent() {
        parent.mkdir(MkdirOptions::WithParents)?;
    }
    let mut source_file = match File::create_new(&source_file_path) {
        Err(err) => {
            if matches!(err.kind(), ErrorKind::AlreadyExists) {
                ctx.console().note_verbose(format!(
                    "the source file {} already exists, not overwriting it",
                    source_file_path.display()
                ))?;
                return Ok(());
            } else {
                return Err(err).context("failed to create the default source file");
            }
        }
        Ok(source_file) => source_file,
    };
    source_file
        .write_all(DEFAULT_SOURCE_CONTENTS.as_bytes())
        .context("failed to write a default duckling file")?;
    Ok(())
}

/// Initialize git repository in the project and add `.gitignore`.
fn init_git(ctx: &DuckContext, root_path: &Path) -> QuackResult<()> {
    Repository::init(root_path).context("failed to initialize a git repository")?;
    let gitignore_path = root_path.join(".gitignore");
    if let Some(parent) = gitignore_path.parent() {
        parent.mkdir(MkdirOptions::WithParents)?;
    }
    let mut gitignore_file = match File::create_new(&gitignore_path) {
        Err(err) => {
            if matches!(err.kind(), ErrorKind::AlreadyExists) {
                ctx.console().note_verbose(format!(
                    "the file {} already exists, not overwriting it",
                    gitignore_path.display()
                ))?;
                return Ok(());
            } else {
                return Err(err).context("failed to create the default `.gitignore` file");
            }
        }
        Ok(gitignore_file) => gitignore_file,
    };
    gitignore_file
        .write_all(DEFAULT_GITIGNORE.as_bytes())
        .context("failed to write to a `.gitignore` file")?;
    Ok(())
}
