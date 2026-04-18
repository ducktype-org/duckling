use std::{ffi::OsString, path::Path, process::Command};

use crate::{QuackResult, QuackResultContext, qp_bail};

/// Execute a .exe file.
pub fn run_exe(path: &Path, args: Vec<OsString>) -> QuackResult<()> {
    let mut command = Command::new(path);
    command.args(args);
    let code = command
        .status()
        .context("failed to run the produced binary")?;
    if !code.success() {
        qp_bail!("binary did not finish successfully")
    }
    Ok(())
}

/// Execute a .dvm file.
pub fn run_dvm() {
    todo!("TODO: #2443")
}
