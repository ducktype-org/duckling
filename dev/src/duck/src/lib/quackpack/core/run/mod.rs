use std::ffi::OsString;
use std::path::Path;
use std::process::Command;

use crate::quackpack::core::compile::executor::ExecutorOutput;
use crate::quackpack::core::compile::unit::ArtifactsType;
use crate::{QuackResult, QuackResultContext, qp_bail_internal};

/// Execute an executable file (either .exe or .dbc).
pub fn run(output: ExecutorOutput, args: Vec<OsString>) -> QuackResult<()> {
    let (unit, path) = output.root;
    match unit.artifacts_type() {
        ArtifactsType::Binary => run_exe(&path, args),
        ArtifactsType::Dvm => unimplemented!("Duckc does not export an API for running dbc"),
        _ => qp_bail_internal!("tried to execute not executable artifacts type"),
    }
}

/// Execute a .exe file.
fn run_exe(path: &Path, args: Vec<OsString>) -> QuackResult<()> {
    let mut command = Command::new(path);
    command.args(args);
    let exit_status = command
        .status()
        .context("failed to run the produced binary")?;
    std::process::exit(exit_status.code().unwrap_or(0))
}
