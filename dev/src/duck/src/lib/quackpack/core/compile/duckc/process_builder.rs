//! [`Command`]-based backend communicating with the compiler.

use std::convert::Infallible;
use std::fmt;
use std::path::Path;
use std::process::{Command, ExitStatus};

use tracing::{debug, info, trace, warn};

use super::Duckc;
use crate::quackpack::core::Package;
use crate::quackpack::core::compile::profiles::OptLevel;
use crate::util::command_ext::CommandExt;
use crate::{QuackResult, QuackResultContext, qp_bail_internal};

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
    fn as_argument(self) -> &'static str {
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
        debug!(%duckc.program_name, "creating new duckc process wrapper");
        Self {
            inner: Command::new(duckc.program_name),
        }
    }

    /// Set [`DuckcSubcommand`] as a main subcommand.
    pub fn set_subcommand(&mut self, subcmd: DuckcSubcommand) -> &mut Self {
        trace!(?subcmd, "setting subcommand");
        self.inner.arg(subcmd.as_argument());
        self
    }

    /// Set path to the manifest.
    pub fn set_manifest_path(&mut self, path: &Path) -> &mut Self {
        trace!(?path, "setting manifest path");
        self.inner.arg(path);
        self
    }

    /// Set path to the script to compile.
    pub fn set_script_path(&mut self, path: &Path) -> &mut Self {
        trace!(?path, "setting script path");
        self.inner.arg(path);
        self
    }

    /// Set package name of the currently compiling package.
    pub fn set_package_name(&mut self, package: &Package) -> &mut Self {
        let name = package.manifest().name();
        trace!(?name, "setting package name");
        self.inner.arg("-n").arg(name);
        self
    }

    /// Set source directory of the currently compiling package.
    pub fn set_src_dir(&mut self, package: &Package) -> QuackResult<&mut Self> {
        let Some(source_directory) = package.source_directory() else {
            qp_bail_internal!("asked for src directory of the global package: {package:#?}")
        };
        trace!(?source_directory, "setting source directory");
        self.inner.arg(source_directory);
        Ok(self)
    }

    /// Set the number of workers to be used by duckc.
    pub fn set_workers_count(&mut self, workers: usize) -> &mut Self {
        if workers == 0 {
            warn!("attempted to set the worker count to 0, ignoring");
            return self;
        }

        if workers == 1 {
            debug!("attempted to set the worker count to 1, ignoring");
            return self;
        }
        trace!(?workers, "setting workers count");
        self.inner.arg("--workers").arg(workers.to_string());
        self
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_package_artifacts_dir(&mut self, package: &Package) -> &mut Self {
        let dir = package.artifacts_directory();
        self.set_artifacts_dir(dir)
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_artifacts_dir(&mut self, dir: &Path) -> &mut Self {
        trace!(?dir, "setting artifacts directory");
        self.inner.arg("-a").arg(dir);
        self
    }

    /// Set LLVM optimization level.
    pub fn set_opt_level(&mut self, opt_level: OptLevel) -> &mut Self {
        trace!(?opt_level, "setting opt level");
        self.inner.arg("-O").arg(opt_level.to_string());
        self
    }

    /// Set to use DVM as the backend.
    pub fn set_dvm_backend(&mut self, value: bool) -> &mut Self {
        trace!(will_use_dvm = %value, "setting dvm");
        if value {
            self.inner.arg("--dvm-backend");
        }
        self
    }

    /// Set not to use cached compilation artifacts.
    pub fn set_incremental(&mut self, value: bool) -> &mut Self {
        trace!(will_use_incremental = %value, "setting incremental");
        if !value {
            self.inner.arg("--no-incremental");
        }
        self
    }

    /// Set not to link c standard library.
    pub fn set_c_std(&mut self, value: bool) -> &mut Self {
        trace!(will_use_c_std = %value, "setting libc");
        if !value {
            self.inner.arg("--no-c-standard-library");
        }
        self
    }

    /// Execute the built command.
    pub fn execute(&mut self) -> QuackResult<ExitStatus> {
        info!(duckc = ?self, "executing duckc");
        self.inner.status().context("failed to spawn duckc")
    }

    /// Execute the built command by replacing current process.
    pub fn execute_and_replace(&mut self) -> QuackResult<Infallible> {
        info!(duckc = ?self, "replacing with duckc");
        self.inner.exec_replace().context("failed to spawn duckc")
    }
}

impl fmt::Display for DuckcProcessBuilder {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.inner.display().fmt(f)
    }
}
