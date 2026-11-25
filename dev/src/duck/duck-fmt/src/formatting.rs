// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Main logic for formatting a package.

use std::convert::Infallible;
use std::ops::ControlFlow;
use std::path::PathBuf;

use clap::ArgMatches;

use crate::duckfmt::DuckFmt;
use crate::fmt_config::package_fmt_config_path;
use crate::package_root::find_package_root;
use crate::util::filesystem::{TryFileVisitor, visit_duckling_source_files};
use crate::{DuckContext, QuackResult};

#[derive(Debug)]
/// Options of `duck-fmt`.
pub struct FormatOptions {
    pub check: bool,
    pub print: bool,
    pub config_path: Option<PathBuf>,
}

impl FormatOptions {
    /// Creates [`FormatOptions`] from [`ArgMatches`].
    pub fn from_matches(matches: &ArgMatches) -> Self {
        Self {
            check: matches.get_flag("check"),
            print: matches.get_flag("print"),
            config_path: matches.get_one("config").cloned(),
        }
    }
}

#[derive(Debug)]
/// Struct responsible for formatting a single package, given [`FormatOptions`].
pub struct PackageFormatter {
    /// Root of the package.
    root: PathBuf,
    /// [`FormatOptions`] to be used.
    options: FormatOptions,
}

#[derive(Debug)]
/// Struct responsible for formatting an arbitrary list of files given [`FormatOptions`].
pub struct FilesFormatter {
    /// Files to format.
    files: Vec<PathBuf>,
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
    pub fn format(self) -> QuackResult<()> {
        let mut duckfmt_command = DuckFmt::new().process_builder();
        duckfmt_command.set_check(self.options.check);
        duckfmt_command.set_print(self.options.print);

        let files = self.collect_source_files()?;
        if files.is_empty() {
            // Nothing to do.
            return Ok(());
        }
        duckfmt_command.add_files_to_format(&files);

        // Config from [`FormatOptions`] is more important than package's config.
        let config_path = self.options.config_path.or_else(|| {
            let pkg_config_path = package_fmt_config_path(&self.root);
            if pkg_config_path.exists() {
                Some(pkg_config_path)
            } else {
                None
            }
        });
        if let Some(config_path) = config_path {
            duckfmt_command.set_fmt_config_path(&config_path);
        }
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

            fn visit(&mut self, path: PathBuf) -> QuackResult<ControlFlow<Self::Break>> {
                self.inner.push(path);
                Ok(ControlFlow::Continue(()))
            }
        }

        let src = self.root.join("src");
        let mut collector = SourceFilesCollector { inner: vec![] };
        visit_duckling_source_files(src, &mut collector)?;
        Ok(collector.inner)
    }
}

impl FilesFormatter {
    /// Create a new [`FilesFormatter`].
    pub fn new(files: Vec<PathBuf>, options: FormatOptions) -> FilesFormatter {
        Self { files, options }
    }

    /// Formats the files.
    /// Creates the right instance of `duckfmt` and runs it.
    pub fn format(self) -> QuackResult<()> {
        let mut duckfmt_command = DuckFmt::new().process_builder();
        duckfmt_command.set_check(self.options.check);
        duckfmt_command.set_print(self.options.print);
        duckfmt_command.add_files_to_format(&self.files);

        if let Some(config_path) = self.options.config_path {
            duckfmt_command.set_fmt_config_path(&config_path);
        }

        duckfmt_command.execute_and_replace()?;
        Ok(())
    }
}
