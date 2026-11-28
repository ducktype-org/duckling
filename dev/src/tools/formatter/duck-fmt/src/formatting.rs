// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Main logic for formatting a package.

use std::convert::Infallible;
use std::ffi::{OsStr, OsString};
use std::ops::ControlFlow;
use std::path::PathBuf;

use clap::ArgMatches;

use crate::duckfmt::DuckFmt;
use crate::package_root::find_package_root;
use crate::util::filesystem::{TryFileVisitor, visit_duckling_source_files};
use crate::{DuckContext, QuackResult};

#[derive(Debug)]
/// Options of `duck-fmt`.
pub struct FormatOptions<'a> {
    pub check: bool,
    pub print: bool,
    pub additional_args: Vec<&'a OsStr>,
}

impl<'a> FormatOptions<'a> {
    /// Creates [`FormatOptions`] from [`ArgMatches`].
    pub fn from_matches(matches: &'a ArgMatches) -> Self {
        let args = matches
            .get_many::<OsString>("args")
            .unwrap_or_default()
            .map(OsString::as_os_str)
            .collect();
        Self {
            check: matches.get_flag("check"),
            print: matches.get_flag("print"),
            additional_args: args,
        }
    }
}

#[derive(Debug)]
/// Struct responsible for formatting a single package, given [`FormatOptions`].
pub struct PackageFormatter<'a> {
    /// Root of the package.
    root: PathBuf,
    /// [`FormatOptions`] to be used.
    options: FormatOptions<'a>,
}

impl<'a> PackageFormatter<'a> {
    /// Create a new [`PackageFormatter`].
    /// This searches for the package's root with [`find_package_root`].
    pub fn new(ctx: &DuckContext, options: FormatOptions<'a>) -> QuackResult<Self> {
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
        duckfmt_command.add_files_to_format(&files);
        duckfmt_command.add_raw_args(&self.options.additional_args);
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
