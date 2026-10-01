use std::path::PathBuf;

use clap::builder::ValueParser;
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, multi, optional, subcommand};
use crate::quackpack::subcommands::translate_c::recipe::CBindingsRecipe;
use crate::quackpack::subcommands::translate_c::{NewOptions, RegenOptions, new, regen};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult};

fn values(matches: &ArgMatches, name: &str) -> Vec<String> {
    matches
        .get_many::<String>(name)
        .map(|values| values.cloned().collect())
        .unwrap_or_default()
}

/// Creates parser for the `translate-c` subcommand.
pub fn get_parser() -> Command {
    let no_verify = || {
        flag(
            "no-verify",
            "Skip building the package and checking its layouts against C",
        )
    };
    subcommand("translate-c")
        .about("Generate a package binding a C library installed on this machine")
        .long_about(
            "Generate a package binding a C library installed on this machine.\n\n\
             The package is specific to this machine: its manifest records the flags and library \
             paths found here. Regenerate it with `duck translate-c regen` on every machine \
             instead of committing or sharing it.",
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
                    "pkg-config package providing compiler and linker flags",
                ))
                .arg(
                    multi("cflag", "Extra compiler flag, e.g. `--cflag=-DFOO`")
                        .allow_hyphen_values(true),
                )
                .arg(
                    multi(
                        "library",
                        "Library to link: a `.o`, `.a` or `.so` path, or `-l`/`-L` flags",
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
                .about("Regenerate a package from the `c-bindings` recipe in its manifest")
                .arg(
                    Arg::new("path")
                        .help("Path to the package (defaults to the current directory)")
                        .value_parser(ValueParser::path_buf())
                        .action(ArgAction::Set),
                )
                .arg(no_verify()),
        )
}

/// Logic for executing the `translate-c` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    match matches.subcommand() {
        Some(("new", matches)) => {
            let at = matches
                .get_one::<PathBuf>("path")
                .expect("required by clap")
                .resolve_with_tilde(ctx);
            new(NewOptions {
                ctx,
                at,
                name: matches.get_one::<String>("name").map(String::as_str),
                recipe: CBindingsRecipe {
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
        Some(("regen", matches)) => {
            let at = matches
                .get_one::<PathBuf>("path")
                .cloned()
                .unwrap_or_else(|| PathBuf::from("."))
                .resolve_with_tilde(ctx);
            regen(RegenOptions {
                ctx,
                at,
                verify: !matches.get_flag("no-verify"),
            })
        }
        _ => unreachable!("clap requires a subcommand"),
    }
}
