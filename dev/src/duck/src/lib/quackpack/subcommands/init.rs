// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Initialize a new project.
use std::borrow::Cow;
use std::fs::{File, OpenOptions};
use std::io::{ErrorKind, Write};
use std::path::{Path, PathBuf};
use std::{fmt, io};

use git2::Repository;

use crate::quackpack::core::valid_package_name::{
    is_duckling_keyword, is_duckling_std_name, validate_package_name,
};
use crate::quackpack::core::{PackageLoader, Version};
use crate::util::path_ops::{MkdirOptions, mkdir, open};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_err};

/// Options for initializing a new project.
pub struct InitOptions<'duck, 'matches> {
    pub ctx: &'duck DuckContext,
    /// Root of the project.
    pub at: PathBuf,
    /// Name of the project.
    pub explicit_name: Option<&'matches str>,
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

impl InitOptions<'_, '_> {
    /// Get the initial project name.
    fn initial_project_name(&self) -> QuackResult<Cow<'_, str>> {
        let name = match self.explicit_name {
            Some(explicit) => Cow::Borrowed(explicit),
            None => self
                .at
                .file_name()
                .with_context(|| format!("path `{}` doesn't have a filename", self.at.display()))?
                .to_string_lossy(),
        };
        Ok(name)
    }

    /// Get the [`MetadataConfigBuilder`] from this [`InitOptions`].
    fn metatadata_config_builder(&self) -> QuackResult<MetadataConfigBuilder<'_>> {
        let initial_name = self.initial_project_name()?;
        if self.full {
            MetadataConfigBuilder::new_from_prompts(self.ctx, initial_name)
        } else {
            Ok(MetadataConfigBuilder::default_with_name(initial_name))
        }
    }
}

#[derive(Debug)]
/// Helper struct for gathering metadata about the new project.
struct MetadataConfigBuilder<'a> {
    name: Cow<'a, str>,
    version: Version,
    author: Option<String>,

    created_by_prompts: bool,
}

impl<'a> MetadataConfigBuilder<'a> {
    /// Create a default [`MetadataConfigBuilder`] with the given name.
    fn default_with_name(name: impl Into<Cow<'a, str>>) -> Self {
        Self {
            name: name.into(),
            version: Version::new(1, 0, 0),
            author: None,
            created_by_prompts: false,
        }
    }

    /// Create a [`MetadataConfigBuilder`] by prompting the user.
    fn new_from_prompts(ctx: &DuckContext, default_name: Cow<'_, str>) -> QuackResult<Self> {
        if ctx.verbosity().is_quiet() {
            qp_bail!("cannot create manifest from user input on quiet verbosity");
        }
        let name =
            ctx.prompt_once_with_default("Enter the project's name", default_name.into_owned())?;
        let author = ctx.prompt_once("Enter the project's author")?;
        let version = ctx.prompt_until_valid_with_default(
            "Enter the version of the project",
            Version::default(),
        );
        Ok(Self {
            name: Cow::Owned(name),
            version,
            author: Some(author),
            created_by_prompts: true,
        })
    }
}

impl fmt::Display for MetadataConfigBuilder<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "metadata:")?;
        writeln!(f, "  name: {}", self.name)?;
        writeln!(f, "  version: '{}'", self.version)?;
        if let Some(ref author) = self.author {
            writeln!(f, "  authors: ['{author}']")?;
        }
        Ok(())
    }
}

#[derive(Default)]
/// A helper for building a partial [`VenvConfig`](crate::quackpack::core::manifest::VenvConfig).
struct VenvConfigBuilder {
    expose_freezefile: Option<bool>,
    ephemeral: Option<bool>,
    storage: Option<PathBuf>,
}

impl fmt::Display for VenvConfigBuilder {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
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

const DEFAULT_SOURCE_FILENAME: &str = "src.dk";

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
    let config = opts.metatadata_config_builder()?;
    validate_new_package_name(
        opts.ctx,
        &config.name,
        /* inferred */ has_inferred_name(&opts, &config),
    )?;
    create_manifest_file(opts.ctx, &opts.at, &config)?;
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
    opts.ctx.info(format!(
        "successfully created a new project `{}` at `{}`",
        config.name,
        opts.at.display()
    ))?;
    Ok(())
}

/// Returns `true` if the name has been inferred from a path.
fn has_inferred_name(opts: &InitOptions<'_, '_>, config: &MetadataConfigBuilder<'_>) -> bool {
    if !config.created_by_prompts {
        return opts.explicit_name.is_none();
    }
    // Name has been created by prompt. Check if it matches the default.
    opts.initial_project_name()
        .is_ok_and(|name| name == config.name)
}

/// Validate a name of the new package.
///
/// _Weird_ names (std/keywords) are warnings instead of errors.
fn validate_new_package_name(ctx: &DuckContext, name: &str, inferred: bool) -> QuackResult<()> {
    validate_package_name(name).with_context(|| {
        if inferred {
            format!("cannot create a project with an invalid inferred name `{name}`")
        } else {
            format!("cannot create a project with an invalid explicit name `{name}`")
        }
    })?;
    if is_duckling_std_name(name) {
        ctx.warning(format!(
            "initializing a project with name `{name}` can have weird effects, as it's one of the packages from the Duckling standard library"
        ))?;
    }
    if is_duckling_keyword(name) {
        ctx.warning(format!(
            "initializing a project with name `{name}` can have weird effects, as it's a Duckling keyword"
        ))?;
    }
    Ok(())
}

/// Create a file with the manifest of the project.
fn create_manifest_file(
    ctx: &DuckContext,
    root_path: &Path,
    config: &MetadataConfigBuilder<'_>,
) -> QuackResult<()> {
    let manifest_path = root_path.join(PackageLoader::MANIFEST_NAME);
    let manifest_contents = config.to_string();
    if let Some(parent) = manifest_path.parent() {
        mkdir(parent, MkdirOptions::WithParents)?;
    }
    let opts = {
        let mut opts = OpenOptions::new();
        opts.write(true).create_new(true);
        opts
    };
    let mut manifest_file = match open(manifest_path, &opts) {
        Err(err) => {
            if let Some(io_err) = err.downcast_ref_in_chain::<io::Error>()
                && matches!(io_err.kind(), ErrorKind::AlreadyExists)
            {
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
        mkdir(parent, MkdirOptions::WithParents)?;
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
/// Currently makes the project's main entry point a `.dk` file with a `main()` function.
/// In the future an option should be added to initialize the project's entry point as a script (`main.dks`).
fn add_package_structure(ctx: &DuckContext, root_path: &Path) -> QuackResult<()> {
    let source_file_path = root_path.join("src").join(DEFAULT_SOURCE_FILENAME);
    if let Some(parent) = source_file_path.parent() {
        mkdir(parent, MkdirOptions::WithParents)?;
    }
    let mut source_file = match File::create_new(&source_file_path) {
        Err(err) => {
            if matches!(err.kind(), ErrorKind::AlreadyExists) {
                ctx.note_verbose(format!(
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
        mkdir(parent, MkdirOptions::WithParents)?;
    }
    let mut gitignore_file = match File::create_new(&gitignore_path) {
        Err(err) => {
            if matches!(err.kind(), ErrorKind::AlreadyExists) {
                ctx.note_verbose(format!(
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
