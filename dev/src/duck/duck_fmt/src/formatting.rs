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

use crate::duckfmt::DuckFmt;
use crate::fmt_config::package_fmt_config_path;
use crate::package_root::find_package_root;
use crate::util::filesystem::{TryFileVisitor, visit_duckling_source_files};
use crate::{DuckContext, QuackResult};

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
pub struct PackageFormatter {
    /// Root of the package.
    root: PathBuf,
    /// [`FormatOptions`] to be used.
    options: FormatOptions,
}

impl PackageFormatter {
    /// Create a new [`PackageFormatter`].
    /// This searches for the package's root with [`find_package_root`].
    pub fn new(ctx: &DuckContext, options: FormatOptions) -> QuackResult<Self> {
        let root = find_package_root(ctx.cwd())?;
        Ok(Self { root, options })
    }

    /// Formats the package.
    /// Creates the right instance of `duckfmt` and runs it.
    pub fn format_package(self) -> QuackResult<()> {
        let mut duckfmt_command = DuckFmt::new().process_builder();
        duckfmt_command.set_check(self.options.check);
        duckfmt_command.set_print(self.options.print);

        let fmt_config_path = self.fmt_config_path();
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

        let src = self.root.join("src");
        let mut collector = SourceFilesCollector { inner: vec![] };
        visit_duckling_source_files(&src, &mut collector)?;
        Ok(collector.inner)
    }

    /// Get the potential localization of the `duckfmt` config file.
    fn fmt_config_path(&self) -> PathBuf {
        package_fmt_config_path(&self.root)
    }
}
