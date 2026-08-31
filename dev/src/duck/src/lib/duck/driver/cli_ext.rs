use std::any::Any;

use clap::{Arg, ArgAction, ArgMatches, Command, value_parser};
use tracing::{error, info};

use crate::StrId;
use crate::quackpack::core::Package;

const DEFAULT_PROFILE: &str = "dev";

pub trait CommandExt: Sized {
    fn _arg_impl(self, arg: Arg) -> Self;

    /// Adds `--profile` flag, conflicting with `--release`.
    fn add_profile(self) -> Self {
        self._arg_impl(
            optional("profile", "Select the compilation profile").conflicts_with("release"),
        )
    }

    /// Adds `--release` flag, conflicting with `--profile`.
    fn add_release(self) -> Self {
        self._arg_impl(flag("release", "Alias for `--profile=release`").conflicts_with("profile"))
    }

    /// Adds `-j`/`--jobs` flags, for specifying number of threads to use.
    fn add_jobs(self) -> Self {
        self._arg_impl(
            optional(
                "jobs",
                "Specify the number of threads to use by the compiler",
            )
            .short('j')
            .value_name("N")
            .value_parser(value_parser!(usize)),
        )
    }
}

impl CommandExt for Command {
    fn _arg_impl(self, arg: Arg) -> Self {
        self.arg(arg)
    }
}

/// Create a new boolean argument.
pub fn flag(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name)
        .help(help)
        .long(name)
        .action(ArgAction::SetTrue)
}

/// Create a new optional flag.
pub fn optional(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name).help(help).long(name).action(ArgAction::Set)
}

/// Create an argument which takes multiple values.
pub fn multi(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name)
        .help(help)
        .long(name)
        .action(ArgAction::Append)
}

/// Create a new subcommand.
pub fn subcommand(name: &'static str) -> Command {
    Command::new(name)
}

/// Get selected profile from `args`.
/// Note:
/// -----
/// This function is only for selecting the profile for building/running packages.
/// Running scripts uses different logic.
pub fn profile_from_matches(args: &ArgMatches) -> StrId {
    if args.get_flag("release") {
        "release".into()
    } else if let Some(profile) = args.get_one::<String>("profile") {
        profile.into()
    } else {
        DEFAULT_PROFILE.into()
    }
}

/// Get enabled features from `args` for package `pkg`.
pub fn features_from_matches(args: &ArgMatches, pkg: &Package) -> Vec<StrId> {
    if args.get_flag("all-features") {
        pkg.manifest()
            .features()
            .all_features()
            .keys()
            .copied()
            .collect()
    } else if let Some(cli_features) = args.get_many::<String>("features") {
        cli_features.map(StrId::from).collect()
    } else {
        vec![]
    }
}

/// Get the number of `jobs` from `args`.
pub fn jobs_from_matches(args: &ArgMatches) -> usize {
    let jobs = args.get_one("jobs");
    match jobs {
        None => 1,
        Some(0) => get_available_parallelism(),
        Some(n) => *n,
    }
}

/// Get the number of available threads in the system.
/// This will return 1 in case of an error.
fn get_available_parallelism() -> usize {
    match std::thread::available_parallelism() {
        Ok(value) => value.get(),
        Err(e) => {
            error!(error = %e, "failed to determine number of available threads");
            info!("falling back to 1 available thread");
            1
        }
    }
}

pub trait ArgMatchesExt {
    /// Safe wrapper around [`get_flag`](ArgMatches::get_flag), with a fallback.
    fn safe_get_flag(&self, name: &str) -> bool;
    /// Safe wrapper around [`try_get_one`](ArgMatches::try_get_one), with a fallback.
    fn safe_get_one<T: Any + Send + Sync + Clone + Default + 'static>(&self, id: &str) -> T;
}

impl ArgMatchesExt for ArgMatches {
    fn safe_get_flag(&self, name: &str) -> bool {
        ignore_clap_errors(self.try_get_one::<bool>(name))
            .copied()
            .unwrap_or_default()
    }

    fn safe_get_one<T: Any + Send + Sync + Clone + Default + 'static>(&self, id: &str) -> T {
        ignore_clap_errors(self.try_get_one(id))
            .cloned()
            .unwrap_or_default()
    }
}

#[track_caller]
/// Ignore [`UnknownArgument`](clap::parser::MatchesError::UnknownArgument), returning the default value,
/// and panic on other errors.
fn ignore_clap_errors<T: Default>(result: Result<T, clap::parser::MatchesError>) -> T {
    match result {
        Ok(val) => val,
        Err(clap::parser::MatchesError::UnknownArgument { .. }) => T::default(),
        Err(e) => panic!("cli flag used incorrectly: {e}"),
    }
}
