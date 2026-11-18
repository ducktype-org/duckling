// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Main logic for formatting a package.

use std::convert::Infallible;
use std::ops::ControlFlow;
use std::path::{Path, PathBuf};

use clap::ArgMatches;
use duck::quackpack::core::{AllowGlobalPackage, PackageContext, PackageLoader, TryFileVisitor};
use duck::{DuckContext, QuackResult};

use crate::duckfmt::DuckFmt;
use crate::fmt_config::AnyPackageFmtExt;

/// Options of `duck-fmt`.
pub struct FormatOptions {
    pub check: bool,
    pub print: bool,
}

impl FormatOptions {
    /// Creates [`FormatOptions`] from [`ArgMatches`].
    pub fn from_matches(matches: &ArgMatches) -> Self {
        Self {
            check: matches.get_flag("check"),
            print: matches.get_flag("print"),
        }
    }
}

/// Struct responsible for formatting a single package, given [`FormatOptions`].
pub struct PackageFormatter<'duck> {
    /// [`PackageContext`] of the package.
    pcx: PackageContext<'duck>,
    /// [`FormatOptions`] to be used.
    options: FormatOptions,
}

impl<'duck> PackageFormatter<'duck> {
    /// Create a new [`PackageFormatter`].
    ///
    /// Note
    /// ----
    /// This calls [`PackageLoader::find_from_cwd`] with [`AllowGlobalPackage::No`],
    /// as global package has no code, so it makes no sense to format it.
    pub fn new(ctx: &'duck DuckContext, options: FormatOptions) -> QuackResult<Self> {
        let pcx = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?;
        Ok(Self { pcx, options })
    }

    /// Formats the package.
    /// Creates the right instance of `duckfmt` and runs it.
    pub fn format_package(self) -> QuackResult<()> {
        let mut duckfmt_command = DuckFmt::new().process_builder();
        duckfmt_command.set_check(self.options.check);
        duckfmt_command.set_print(self.options.print);

        let fmt_config_path = self.pcx.package().fmt_config_path()?;
        if fmt_config_path.exists() {
            duckfmt_command.set_fmt_config_path(&fmt_config_path);
        }
        let files = self.collect_source_files()?;
        if files.is_empty() {
            // Nothing to do.
            return Ok(());
        }
        duckfmt_command.add_files_to_format(&files);
        duckfmt_command.execute_and_replace()?;
        Ok(())
    }

    /// Collects all source files inside the package, so that we can add them to `duckfmt` instance.
    pub fn collect_source_files(&self) -> QuackResult<Vec<PathBuf>> {
        struct SourceFilesCollector {
            inner: Vec<PathBuf>,
        }
        impl TryFileVisitor for SourceFilesCollector {
            type Break = Infallible;

            fn visit(&mut self, path: &Path) -> QuackResult<ControlFlow<Self::Break>> {
                self.inner.push(path.to_path_buf());
                Ok(ControlFlow::Continue(()))
            }
        }

        let mut collector = SourceFilesCollector { inner: vec![] };
        self.pcx
            .package()
            .try_visit_all_source_files(&mut collector)?;
        Ok(collector.inner)
    }
}
