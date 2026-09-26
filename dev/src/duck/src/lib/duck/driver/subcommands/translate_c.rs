use std::io;
use std::process::Command as Process;

use clap::{Arg, ArgAction, ArgMatches, Command};
use tracing::info;

use crate::duck::driver::cli_ext::{flag, multi, optional, subcommand};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail};

/// Name of the helper doing the actual translation, resolved through `$PATH`.
const HELPER: &str = "duck_c_import";

/// Creates parser for the `translate-c` subcommand.
pub fn get_parser() -> Command {
    subcommand("translate-c")
        .about("Generate a Duckling package from C headers")
        .arg(
            Arg::new("package-name")
                .help("Name of the generated package")
                .required(true)
                .action(ArgAction::Set),
        )
        .arg(
            multi(
                "header",
                "C header to translate. May be given more than once",
            )
            .required(true),
        )
        .arg(optional(
            "out-dir",
            "Directory to write the package to. Defaults to the package name",
        ))
        .arg(optional(
            "library",
            "Object or archive to link against. Should be an absolute path",
        ))
        .arg(
            // The value is linker arguments, so it starts with a hyphen more often than not.
            Arg::new("links-raw")
                .help("Linker arguments to use verbatim instead of `--library`")
                .long("links-raw")
                .allow_hyphen_values(true)
                .action(ArgAction::Set),
        )
        .arg(multi(
            "dvm-shared-lib",
            "Shared object the DVM loads at runtime",
        ))
        .arg(optional("version", "Version written to the manifest"))
        .arg(optional("std", "C standard passed to clang"))
        .arg(flag(
            "no-verify",
            "Skip compiling the generated package to check that it is valid",
        ))
        .arg(optional(
            "duckc",
            "Compiler used for that check. Defaults to `duckc` on the PATH",
        ))
        .arg(flag(
            "split",
            "Emit one module per translated header instead of a single one",
        ))
        .arg(flag(
            "force",
            "Overwrite the output directory when it is not empty",
        ))
        .arg(flag(
            "ignore-parse-errors",
            "Generate bindings even when clang reports errors",
        ))
        .arg(
            Arg::new("clang-args")
                .help("Arguments passed to clang, after a `--`")
                .last(true)
                .num_args(..)
                .allow_hyphen_values(true)
                .action(ArgAction::Append),
        )
}

/// Logic for executing the `translate-c` subcommand.
pub fn execute(_ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let mut process = Process::new(HELPER);

    // `package-name` and `header` are required, so both are always present.
    process.arg("--package-name");
    process.arg(matches.get_one::<String>("package-name").unwrap());

    // The translator takes lists comma separated, while `--header` is repeatable here.
    let headers: Vec<&str> = matches
        .get_many::<String>("header")
        .unwrap()
        .map(String::as_str)
        .collect();
    process.arg("--header").arg(headers.join(","));

    for (name, key) in [
        ("--out-dir", "out-dir"),
        ("--library", "library"),
        ("--links-raw", "links-raw"),
        ("--version", "version"),
        ("--std", "std"),
        ("--duckc", "duckc"),
    ] {
        if let Some(value) = matches.get_one::<String>(key) {
            process.arg(name).arg(value);
        }
    }

    if let Some(libraries) = matches.get_many::<String>("dvm-shared-lib") {
        let libraries: Vec<&str> = libraries.map(String::as_str).collect();
        process.arg("--dvm-shared-lib").arg(libraries.join(","));
    }

    for name in ["split", "force", "ignore-parse-errors", "no-verify"] {
        if matches.get_flag(name) {
            process.arg(format!("--{name}"));
        }
    }

    if let Some(clang_args) = matches.get_many::<String>("clang-args") {
        process.arg("--");
        for argument in clang_args {
            process.arg(argument);
        }
    }

    info!(helper = HELPER, "spawning the C header translator");

    let status = process
        .status()
        .context("failed to spawn the C header translator")
        .map_err(add_path_hint_to_missing_helper)?;

    if !status.success() {
        qp_bail!("{HELPER} failed");
    }

    Ok(())
}

fn add_path_hint_to_missing_helper(error: QuackError) -> QuackError {
    if let Some(io_err) = error.downcast_ref_in_chain::<io::Error>()
        && io_err.kind() == io::ErrorKind::NotFound
    {
        error.add_hint(format!("try adding {HELPER} to the $PATH"))
    } else {
        error
    }
}
