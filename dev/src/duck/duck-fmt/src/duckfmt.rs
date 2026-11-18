// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Low-level creator of `duckfmt` instance.

use std::convert::Infallible;
use std::io;
use std::path::{Path, PathBuf};
use std::process::Command;

use duck::util::command_ext::CommandExt;
use duck::{QuackError, QuackResult, QuackResultContext, StrId};

#[derive(Debug)]
/// Data holder of all required in order to execute the formatter.
pub struct DuckFmt {
    program_name: StrId,
}

impl DuckFmt {
    /// Create new [`DuckFmt`].
    pub fn new() -> Self {
        Self {
            program_name: "duckfmt".into(),
        }
    }

    /// Get [`DuckFmtProcessBuilder`].
    pub fn process_builder(&self) -> DuckFmtProcessBuilder {
        DuckFmtProcessBuilder::new(self)
    }
}

#[derive(Debug)]
/// Wrapper around [`Command`], handles specific duckc options.
pub struct DuckFmtProcessBuilder {
    inner: Command,
}

impl DuckFmtProcessBuilder {
    /// Create new [`DuckFmtProcessBuilder`] from the data in [`DuckFmt`].
    pub fn new(duckfmt: &DuckFmt) -> Self {
        Self {
            inner: Command::new(duckfmt.program_name),
        }
    }

    /// Check the result (return error if its unformatted).
    pub fn set_check(&mut self, check: bool) -> &mut Self {
        if check {
            self.inner.arg("-k");
        }
        self
    }

    /// Print the formatted results to stdout.
    pub fn set_print(&mut self, print: bool) -> &mut Self {
        // `duckfmt` prints by default.
        if !print {
            self.inner.arg("-i");
        }
        self
    }

    /// Set the `duckfmt` configuration file.
    pub fn set_fmt_config_path(&mut self, path: &Path) -> &mut Self {
        self.inner.arg("-c").arg(path);
        self
    }

    /// Add files to format for `duckfmt`.
    pub fn add_files_to_format(&mut self, paths: &[PathBuf]) -> &mut Self {
        for path in paths {
            self.inner.arg(path);
        }
        self
    }

    /// Execute the built command by replacing current process.
    pub fn execute_and_replace(&mut self) -> QuackResult<Infallible> {
        self.inner
            .exec_replace()
            .context("failed to spawn duckfmt")
            .map_err(add_path_hint_to_missing_duckfmt)
    }
}

/// Add a hint about adding `duckfmt` to path.
fn add_path_hint_to_missing_duckfmt(mut error: QuackError) -> QuackError {
    if let Some(io_err) = error.downcast_ref_in_chain::<io::Error>()
        && io_err.kind() == io::ErrorKind::NotFound
    {
        error.add_hint("try adding duckfmt to the $PATH");
        error
    } else {
        error
    }
}
