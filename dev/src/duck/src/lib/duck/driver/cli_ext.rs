use clap::{Arg, ArgAction, ArgMatches, Command, ValueHint, builder::ValueParser};

use crate::{StrId, quackpack::core::Package};

const DEFAULT_PROFILE: &str = "dev";

pub trait CommandExt: Sized {
    fn _arg_impl(self, arg: Arg) -> Self;

    /// Same as [`add_features`](Self::add_features), but `-F`/`--features` flag conflicts with
    /// `with`.
    fn add_features_conflicting(self, help: &'static str, with: &'static str) -> Self {
        self._arg_impl(multi("features", help).short('F').conflicts_with(with))
    }

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

    /// Adds `-v`/`--verbose` flags, conflicting with `--quiet`.
    fn add_verbose(self) -> Self {
        self._arg_impl(
            flag("verbose", "Use more verbose output")
                .conflicts_with("quiet")
                .short('v'),
        )
    }

    /// Adds `-q`/`--quiet` flags, conflicting with `--verbose`.
    fn add_quiet(self) -> Self {
        self._arg_impl(
            flag("quiet", "Suppress all output")
                .short('q')
                .conflicts_with("verbose"),
        )
    }

    /// Adds `-C`/`--directory` flag, for changing the current directory before making any actions.
    fn add_chdir(self) -> Self {
        self._arg_impl(
            optional(
                "directory",
                "Change to <DIRECTORY> before performing any actions",
            )
            .value_name("DIRECTORY")
            .value_parser(ValueParser::path_buf())
            .value_hint(ValueHint::DirPath)
            .short('C'),
        )
    }

    /// Adds `--color` flag.
    fn add_color(self) -> Self {
        self._arg_impl(
            optional("color", "Control the colored output")
                .value_parser(["always", "never", "auto"])
                .default_value("auto"),
        )
    }

    /// Adds `--offline` flag.
    fn add_offline(self) -> Self {
        self._arg_impl(flag("offline", "Don't perform any network requests"))
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
            .value_parser(1..),
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
    Arg::new(name).help(help).action(ArgAction::Append)
}

/// Create a new subcommand.
pub fn subcommand(name: &'static str) -> Command {
    Command::new(name)
}

/// Get selected profile from `args`.
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
