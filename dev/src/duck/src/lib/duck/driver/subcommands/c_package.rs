// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::PathBuf;

use clap::builder::ValueParser;
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, multi, optional, subcommand};
use crate::quackpack::subcommands::c_package::recipe::Recipe;
use crate::quackpack::subcommands::c_package::{NewOptions, check, new, regen};
use crate::util::path_ops::resolve_with_tilde;
use crate::{DuckContext, QuackResult};

fn values(matches: &ArgMatches, name: &str) -> Vec<String> {
    matches
        .get_many::<String>(name)
        .map(|values| values.cloned().collect())
        .unwrap_or_default()
}

fn package_path_arg() -> Arg {
    Arg::new("path")
        .help("Path to the package (defaults to the current directory)")
        .value_parser(ValueParser::path_buf())
        .action(ArgAction::Set)
}

fn package_path(matches: &ArgMatches, ctx: &DuckContext) -> PathBuf {
    resolve_with_tilde(
        matches
            .get_one::<PathBuf>("path")
            .cloned()
            .unwrap_or_else(|| PathBuf::from(".")),
        ctx,
    )
}

/// Creates parser for the `c-package` subcommand.
pub fn get_parser() -> Command {
    let no_verify = || {
        flag(
            "no-verify",
            "Skip building the package and checking its layouts against C",
        )
    };
    subcommand("c-package")
        .about("Generate a package binding a C library")
        .long_about(
            "Generate a package binding a C library.\n\n\
             The recipe is stored in `c-package.yaml` in the package. The package names its \
             libraries in `metadata.external-libs`, and duck finds them when it is built, so the \
             package can be published.",
        )
        .subcommand_required(true)
        .subcommand(
            subcommand("new")
                .about("Create a package from C headers")
                .arg(
                    Arg::new("path")
                        .help("Path to the new package")
                        .value_parser(ValueParser::path_buf())
                        .required(true)
                        .action(ArgAction::Set),
                )
                .arg(optional("name", "Override the package name"))
                .arg(
                    multi(
                        "header",
                        "Header to translate (a path, or e.g. `SDL3/SDL.h`)",
                    )
                    .required(true),
                )
                .arg(multi(
                    "pkg-config",
                    "pkg-config package providing compiler flags and library names",
                ))
                .arg(
                    multi("cflag", "Extra compiler flag, e.g. `--cflag=-DFOO`")
                        .allow_hyphen_values(true),
                )
                .arg(
                    multi(
                        "library",
                        "Library to link: `-l<name>`, `-L<dir>`, or a path to `lib<name>.a`/`.so`",
                    )
                    .allow_hyphen_values(true),
                )
                .arg(multi(
                    "include",
                    "Only translate declarations whose names match this glob",
                ))
                .arg(multi(
                    "exclude",
                    "Leave out declarations whose names match this glob",
                ))
                .arg(no_verify()),
        )
        .subcommand(
            subcommand("regen")
                .about("Regenerate a package from its `c-package.yaml`")
                .arg(package_path_arg())
                .arg(no_verify()),
        )
        .subcommand(
            subcommand("check")
                .about("Check that a package is what its `c-package.yaml` generates")
                .arg(package_path_arg()),
        )
}

/// Logic for executing the `c-package` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    match matches.subcommand() {
        Some(("new", matches)) => {
            let at = resolve_with_tilde(
                matches.get_one::<PathBuf>("path").expect("required by clap"),
                ctx,
            );
            new(NewOptions {
                ctx,
                at,
                name: matches.get_one::<String>("name").map(String::as_str),
                recipe: Recipe {
                    headers: values(matches, "header"),
                    pkg_config: values(matches, "pkg-config"),
                    cflags: values(matches, "cflag"),
                    libraries: values(matches, "library"),
                    include: values(matches, "include"),
                    exclude: values(matches, "exclude"),
                },
                verify: !matches.get_flag("no-verify"),
            })
        }
        Some(("regen", matches)) => regen(
            ctx,
            package_path(matches, ctx),
            !matches.get_flag("no-verify"),
        ),
        Some(("check", matches)) => check(ctx, package_path(matches, ctx)),
        _ => unreachable!("clap requires a subcommand"),
    }
}
