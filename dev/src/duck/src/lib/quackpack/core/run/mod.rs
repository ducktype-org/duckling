use std::convert::Infallible;
use std::ffi::OsStr;
use std::path::Path;
use std::process::Command;

use super::compile::unit_runner::CompilationOutput;
use crate::quackpack::core::compile::unit_runner::CompilationTarget;
use crate::util::command_ext::CommandExt;
use crate::{QuackResult, QuackResultContext};

/// Execute an executable file (either .exe or .dbc).
pub fn run(output: CompilationOutput, args: Vec<&OsStr>) -> QuackResult<Infallible> {
    let (_unit, path) = output.root;
    match output.target {
        CompilationTarget::LLVM => run_exe(&path, args),
        CompilationTarget::DVM => run_dvm(&path, args),
    }
}

/// Execute a .exe file.
fn run_exe(path: &Path, args: Vec<&OsStr>) -> QuackResult<Infallible> {
    let mut command = Command::new(path);
    command.args(args);
    command
        .exec_replace()
        .context("failed to run the produced binary")
}

/// Execute a .dbc file.
fn run_dvm(path: &Path, args: Vec<&OsStr>) -> QuackResult<Infallible> {
    // Assuming that VM is in PATH.
    let mut command = Command::new("VM");
    command.arg("run");
    if !args.is_empty() {
        let args_string = args.join(OsStr::new(","));
        command.arg("-c").arg(args_string);
    }
    command.arg(path);
    command
        .exec_replace()
        .context("failed to run the produced binary")
}
