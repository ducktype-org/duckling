use std::{
    ffi::OsString,
    path::Path,
    process::{Command, ExitStatus},
};

use crate::{QuackResult, QuackResultContext};

/// Execute a .exe file.
pub fn run_exe(path: &Path, args: Vec<OsString>) -> QuackResult<ExitStatus> {
    let mut command = Command::new(path);
    command.args(args);
    command
        .status()
        .context("failed to run the produced binary")
}
