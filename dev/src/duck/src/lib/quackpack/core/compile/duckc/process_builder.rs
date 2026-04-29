//! [`Command`]-based backend communicating with the compiler.

use std::convert::Infallible;
use std::fmt;
use std::path::Path;
use std::process::Command;

use super::Duckc;
use crate::quackpack::core::Package;
use crate::quackpack::core::compile::profiles::{OptLevel, Profile};
use crate::util::command_ext::CommandExt;
use crate::{QuackResult, QuackResultContext, qp_bail};

#[non_exhaustive]
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Supported subcommands passed to the duckc.
pub enum DuckcSubcommand {
    CompilePackage,
    CompileScript,
    Repl,
}

impl DuckcSubcommand {
    fn as_argument(&self) -> &'static str {
        match self {
            Self::CompilePackage => "compile_package",
            Self::CompileScript => "compile_script",
            Self::Repl => "repl",
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
    pub fn set_src_dir(&mut self, package: &Package) -> &mut Self {
        self.inner.arg(package.source_directory());
        self
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_package_artifacts_dir(&mut self, package: &Package) -> &mut Self {
        let dir = package.artifacts_directory();
        self.set_artifacts_dir(dir)
    }

    /// Set artifacts directory of the currently compiling package.
    pub fn set_artifacts_dir(&mut self, dir: &Path) -> &mut Self {
        self.inner.arg("-a").arg(dir);
        self
    }

    /// Sets the following arguments:
    ///  * LLVM opt level,
    ///  * compilation backend,
    ///  * whether to use previous compilation artifacts,
    ///  * whether to link c standard library.
    pub fn update_with_profile(&mut self, profile: &Profile) -> &mut Self {
        self.set_opt_level(profile.opt_level);
        if profile.dvm_bytecode {
            self.set_dvm_backend();
        }
        if !profile.incremental {
            self.set_no_incremental();
        }
        if !profile.c_std {
            self.set_no_c_std();
        }
        self
    }

    /// As [`Self::update_with_profile`] but does not set `no_incremental`.
    pub fn update_with_script_profile(&mut self, profile: &Profile) -> &mut Self {
        self.set_opt_level(profile.opt_level);
        if profile.dvm_bytecode {
            self.set_dvm_backend();
        }
        if !profile.c_std {
            self.set_no_c_std();
        }
        self
    }

    /// Set LLVM optimization level.
    fn set_opt_level(&mut self, opt_level: OptLevel) -> &mut Self {
        self.inner.arg("-O").arg(opt_level.to_string());
        self
    }

    /// Set to use DVM as the backend.
    fn set_dvm_backend(&mut self) -> &mut Self {
        self.inner.arg("--dvm-backend");
        self
    }

    /// Set not to use cached compilation artifacts.
    fn set_no_incremental(&mut self) -> &mut Self {
        self.inner.arg("--no-incremental");
        self
    }

    /// Set not to link c standard library.
    fn set_no_c_std(&mut self) -> &mut Self {
        self.inner.arg("--no-c-standard-library");
        self
    }

    /// Execute the built command.
    pub fn execute<F, T>(&mut self, on_error_message: F) -> QuackResult<()>
    where
        T: fmt::Display,
        F: FnOnce() -> T,
    {
        let code = self.inner.status().context("failed to spawn duckc")?;
        if !code.success() {
            qp_bail!("{}", on_error_message())
        }
        Ok(())
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
