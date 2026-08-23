use std::convert::Infallible;
use std::ffi::OsString;
use std::path::Path;
use std::process::Command;

use clap::builder::OsStr;

use crate::quackpack::core::compile::unit::ArtifactsType;
use crate::quackpack::core::compile::unit_compiler::CompilationOutput;
use crate::util::command_ext::CommandExt;
use crate::{QuackResult, QuackResultContext, qp_bail_internal};

/// Execute an executable file (either .exe or .dbc).
pub fn run(output: CompilationOutput, args: Vec<OsString>) -> QuackResult<Infallible> {
    let (unit, path) = output.root;
    match unit.artifacts_type() {
        ArtifactsType::Binary => run_exe(&path, args),
        ArtifactsType::Dvm => run_dvm(&path, args),
        _ => qp_bail_internal!(
            "tried to execute not executable artifacts type {:?}",
            unit.artifacts_type()
        ),
    }
}

/// Execute a .exe file.
fn run_exe(path: &Path, args: Vec<OsString>) -> QuackResult<Infallible> {
    let mut command = Command::new(path);
    command.args(args);
    command
        .exec_replace()
        .context("failed to run the produced binary")
}

/// Execute a .dbc file.
fn run_dvm(path: &Path, args: Vec<OsString>) -> QuackResult<Infallible> {
    // Assuming that VM is in PATH.
    let mut command = Command::new("VM");
    command.arg("run");
    if !args.is_empty() {
        let args_string = args.join(&OsStr::from(","));
        command.arg("-c").arg(args_string);
    }
    command.arg(path);
    command
        .exec_replace()
        .context("failed to run the produced binary")
}
