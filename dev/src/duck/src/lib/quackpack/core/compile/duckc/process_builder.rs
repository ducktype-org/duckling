//! [`Command`]-based backend communicating with the compiler.

use std::convert::Infallible;
use std::fmt;
use std::path::Path;
use std::process::{Command, ExitStatus};

use tracing::{error, trace};

use super::Duckc;
use crate::quackpack::core::Package;
use crate::quackpack::core::compile::profiles::OptLevel;
use crate::util::command_ext::CommandExt;
use crate::{QuackResult, QuackResultContext};

#[non_exhaustive]
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Supported subcommands passed to the duckc.
pub enum DuckcSubcommand {
    CompilePackage,
    CompileScript,
    CompilePackages,
    Repl,
}

impl DuckcSubcommand {
    fn as_argument(&self) -> &'static str {
        match self {
            Self::CompilePackage => "compile_package",
            Self::CompileScript => "compile_script",
            Self::Repl => "repl",
            Self::CompilePackages => "compile_packages",
        }
    }
}

#[derive(Debug)]
/// Wrapper around [`Command`], handles specific duckc options.
pub struct DuckcProcessBuilder {
    inner: Command,
}

impl DuckcProcessBuilder {
    /// Create new [`DuckcProcessBuilder`] from the data in [`Duckc`].
    pub fn new(duckc: &Duckc) -> Self {
        Self {
            inner: Command::new(duckc.program_name),
        }
    }

    /// Set [`DuckcSubcommand`] as a main subcommand.
    pub fn set_subcommand(&mut self, subcmd: DuckcSubcommand) -> &mut Self {
        self.inner.arg(subcmd.as_argument());
        self
    }

    /// Set path to the manifest.
    pub fn set_manifest_path(&mut self, path: &Path) -> &mut Self {
        self.inner.arg(path);
        self
    }

    /// Set path to the script to compile.
    pub fn set_script_path(&mut self, path: &Path) -> &mut Self {
        self.inner.arg(path);
        self
    }

    /// Set package name of the currently compiling package.
    pub fn set_package_name(&mut self, package: &Package) -> &mut Self {
        let name = package.manifest().name();
        self.inner.arg("-n").arg(name);
        self
    }

    /// Set source directory of the currently compiling package.
    pub fn set_src_dir(&mut self, package: &Package) -> QuackResult<&mut Self> {
        let source_directory = package.source_directory().context_internal(
            "asked for src directory of the global package or a script with frontmatter",
        )?;
        self.inner.arg(source_directory);
        Ok(self)
    }

    /// Set the number of workers to be used by duckc.
    pub fn set_workers_count(&mut self, workers: usize) -> &mut Self {
        if workers == 0 {
            error!("attempted to set the worker count to 0, ignoring");
            return self;
        }

        if workers == 1 {
            trace!("attempted to set the worker count to 1, ignoring");
            return self;
        }
        self.inner.arg("--workers").arg(workers.to_string());
        self
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_package_artifacts_dir(&mut self, package: &Package) -> &mut Self {
        let dir = package.artifacts_directory();
        self.set_artifacts_dir(dir.root_directory())
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_artifacts_dir(&mut self, dir: &Path) -> &mut Self {
        self.inner.arg("-a").arg(dir);
        self
    }

    /// Set LLVM optimization level.
    pub fn set_opt_level(&mut self, opt_level: OptLevel) -> &mut Self {
        self.inner.arg("-O").arg(opt_level.to_string());
        self
    }

    /// Set to use DVM as the backend.
    pub fn set_dvm_backend(&mut self, value: bool) -> &mut Self {
        if value {
            self.inner.arg("--dvm-backend");
        }
        self
    }

    /// Set not to use cached compilation artifacts.
    pub fn set_incremental(&mut self, value: bool) -> &mut Self {
        if !value {
            self.inner.arg("--no-incremental");
        }
        self
    }

    /// Set not to link c standard library.
    pub fn set_c_std(&mut self, value: bool) -> &mut Self {
        if !value {
            self.inner.arg("--no-c-standard-library");
        }
        self
    }

    /// Execute the built command.
    pub fn execute(&mut self) -> QuackResult<ExitStatus> {
        self.inner.status().context("failed to spawn duckc")
    }

    /// Execute the built command by replacing current process.
    pub fn execute_and_replace(&mut self) -> QuackResult<Infallible> {
        self.inner.exec_replace().context("failed to spawn duckc")
    }
}

impl fmt::Display for DuckcProcessBuilder {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.inner.display().fmt(f)
    }
}
