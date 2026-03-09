use clap::{Arg, ArgAction, ArgMatches, Command, ValueHint, builder::ValueParser};

use crate::{StrId, quackpack::core::Package};

const DEFAULT_PROFILE: &str = "debug";

pub trait CommandExt: Sized {
    fn _arg_impl(self, arg: Arg) -> Self;

    fn add_global_venv(self) -> Self {
        self._arg_impl(flag("global", "Add packages to the global venv"))
    }

    fn add_features(self, help: &'static str) -> Self {
        self._arg_impl(multi_optional("features", help).short('F'))
    }

    fn add_features_conflicting(self, help: &'static str, with: &'static str) -> Self {
        self._arg_impl(
            multi_optional("features", help)
                .short('F')
                .conflicts_with(with),
        )
    }

    fn add_packages(self, help: &'static str) -> Self {
        self._arg_impl(multi("packages", help))
    }

    fn add_local_git_deps(self, local_help: &'static str, git_help: &'static str) -> Self {
        self._arg_impl(flag("local", local_help).conflicts_with("git"))
            ._arg_impl(flag("git", git_help).conflicts_with("local"))
    }

    fn add_profile(self) -> Self {
        self._arg_impl(
            optional("profile", "Select the compilation profile").conflicts_with("release"),
        )
    }

    fn add_release(self) -> Self {
        self._arg_impl(flag("release", "Alias for `--profile=release`").conflicts_with("profile"))
    }

    fn add_verbose(self) -> Self {
        self._arg_impl(
            flag("verbose", "Use more verbose output")
                .conflicts_with("quiet")
                .short('v'),
        )
    }

    fn add_quiet(self) -> Self {
        self._arg_impl(
            flag("quiet", "Suppress all output")
                .short('q')
                .conflicts_with("verbose"),
        )
    }

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

    fn add_color(self) -> Self {
        self._arg_impl(
            optional("color", "Control the colored output")
                .value_parser(["always", "never", "auto"])
                .default_value("auto"),
        )
    }

    fn add_offline(self) -> Self {
        self._arg_impl(flag("offline", "Don't perform any network requests"))
    }

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

    fn add_dev(self, dev_help: &'static str) -> Self {
        self._arg_impl(flag("dev", dev_help))
    }
}

impl CommandExt for Command {
    fn _arg_impl(self, arg: Arg) -> Self {
        self.arg(arg)
    }
}

pub fn flag(name: &'static str, help: &'static str) -> Arg {
    optional(name, help).action(ArgAction::SetTrue)
}

pub fn optional(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name).help(help).long(name).action(ArgAction::Set)
}

pub fn multi_optional(name: &'static str, help: &'static str) -> Arg {
    optional(name, help).action(ArgAction::Append)
}

pub fn multi(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name).help(help).action(ArgAction::Append)
}

pub fn subcommand(name: &'static str) -> Command {
    Command::new(name)
}

pub fn profile_from_matches(args: &ArgMatches) -> StrId {
    if args.get_flag("release") {
        "release".into()
    } else if let Some(profile) = args.get_one::<String>("profile") {
        profile.into()
    } else {
        DEFAULT_PROFILE.into()
    }
}

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
