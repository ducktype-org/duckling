//! Initialize a new project.
use std::path::{Path, PathBuf};

use crate::{
    DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, quackpack::core::PackageLoader,
    util::path_ops_ext::PathOpsExt,
};

/// Options for initializing a new project.
pub struct InitOptions<'duck> {
    pub ctx: &'duck DuckContext,
    /// Root of the project.
    pub at: PathBuf,
    /// Name of the project.
    pub name: StrId,
}

const DEFAULT_SOURCE_FILENAME: &str = "src.dmf";

const DEFAULT_SOURCE_CONTENTS: &str = "\
fun main() = {
    # !TODO: On macOS, builtin_output_string segfaults :^);
    # builtin_output_string(\"Hello, world!\");
    return 0;
}
";

/// Initialize a new project with the given options.
pub fn init(opts: InitOptions<'_>) -> QuackResult<()> {
    bail_if_would_override_project(opts.ctx, &opts.at)?;
    let manifest_file = opts.at.join(PackageLoader::MANIFEST_NAME);
    manifest_file
        .touch()
        .context("failed to create a manifest file")?;
    manifest_file
        .write(make_default_manifest_for_name(&opts.name))
        .context("failed to write a default manifest")?;
    let source_file = opts.at.join("src").join(DEFAULT_SOURCE_FILENAME);
    if !source_file.exists() {
        source_file
            .touch()
            .context("failed to create a default source file")?;
        source_file
            .write(DEFAULT_SOURCE_CONTENTS)
            .context("failed to write a default duck file")?;
    }
    opts.ctx.console().info(format!(
        "successfully created new project `{}` at `{}`",
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
