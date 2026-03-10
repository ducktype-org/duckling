//! [`Command`]-based backend communicating with the compiler.

use itertools::Itertools;
use std::{ffi::OsStr, fmt, process::Command};

use crate::{QuackResult, QuackResultContext, qp_bail, quackpack::core::Package};

use super::Duckc;

#[non_exhaustive]
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Supported subcommands passed to the duckc.
pub enum DuckcSubcommand {
    CompilePackage,
}

impl DuckcSubcommand {
    fn as_argument(&self) -> &'static str {
        match self {
            Self::CompilePackage => "compile_package",
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

    /// Set package name of the currently compiling package.
    pub fn set_package_name(&mut self, package: &Package) -> &mut Self {
        let name = package.manifest().root_description().name();
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
        self.inner.arg("-a").arg(dir);
        self
    }

    /// Execute the built command.
    pub fn execute(&mut self, package_name: impl fmt::Display) -> QuackResult<()> {
        let code = self.inner.status().context("failed to spawn duckc")?;
        if !code.success() {
            qp_bail!("failed to compile package `{package_name}`")
        }
        Ok(())
    }
}

impl fmt::Display for DuckcProcessBuilder {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let command_name = self.inner.get_program().display();
        let args = self.inner.get_args().map(OsStr::display).join(" ");
        if !args.is_empty() {
            write!(f, "{command_name} {args}")
        } else {
            write!(f, "{command_name}")
        }
    }
}
