use std::path::PathBuf;
use std::str::FromStr;

use crate::duck::driver::cli_ext::{multi, optional};
use crate::quackpack::core::{DependencyKind, Version};
use crate::quackpack::subcommands::add::{
    AddOptions, DependencySpecification, NameSpecification, SourceSpecification, add,
};
use crate::{DuckContext, QuackResult};
use clap::{Arg, ArgGroup, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};

/// Creates parser for the `add` subcommand.
pub fn get_parser() -> Command {
    // Conflict are defined through groups.
    subcommand("add")
        .about("Add a dependency to the current venv")
        .arg(flag("global", "Add a dependency from the global venv instead").short('g'))
        .arg(flag("dev", "Add a dev dependency instead"))
        .arg(flag("pinned", "Pin the dependency's version"))
        .arg(optional("alias", "How to alias the dependency"))
        .arg(optional("local", "Add a local dependency, specified by path"))
        .arg(optional("git", "Add a git dependency, on a repository under a URL"))
        .arg(optional("branch", "Specify a git branch of the dependency"))
        .arg(optional("tag", "Specify a git tag of the dependency"))
        .arg(optional("commit", "Specify a git commit of the dependency. Can be either short or long commit id"))
        .arg(optional("registry", "Specify a registry from which this dependency should be taken"))
        .group(ArgGroup::new("dependency-source").args(["local", "git", "registry"]))
        .group(ArgGroup::new("git-references").args(["branch", "tag", "commit"]).requires("git"))
        .group(ArgGroup::new("not-pinned-sources").args(["local", "git"]).conflicts_with("pinned"))
        .arg(multi("version", "List of versions with which the dependency should be compatible or a single version if `pinned` is set"))
        .arg(multi("features", "Features of the dependency to add"))
        .arg(
            Arg::new("name")
                .help("Name of the dependency to add")
                .required(true),
        )
}

/// Logic for executing the `add` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let alias = matches.get_one::<String>("alias").cloned();
    let name = matches
        .get_one::<String>("name")
        .expect("guarded by the parser")
        .clone();
    let name_spec = NameSpecification { name, alias };
    let source_spec = get_source_specification(matches);
    let versions = matches
        .get_many::<String>("version")
        .into_iter()
        .flatten()
        .map(|v| Version::from_str(v))
        .collect::<QuackResult<Vec<Version>>>()?;
    let features = matches
        .get_many::<String>("features")
        .into_iter()
        .flatten()
        .cloned()
        .collect();
    let dep_spec = DependencySpecification {
        name_spec,
        source_spec,
        versions,
        features,
        pinned: matches.get_flag("pinned"),
    };
    let kind = determine_kind(matches);
    let options = AddOptions {
        dep_spec,
        global: matches.get_flag("global"),
        kind,
    };
    add(ctx, options)
}

/// Construct the [`SourceSpecification`] from the matches.
fn get_source_specification(matches: &ArgMatches) -> SourceSpecification {
    let local_path = matches.get_one::<String>("local").map(PathBuf::from);
    let git_url = matches.get_one("git").cloned();
    let git_tag = matches.get_one("tag").cloned();
    let git_branch = matches.get_one("branch").cloned();
    let git_commit = matches.get_one("commit").cloned();
    let registry_url = matches.get_one("registry").cloned();
    SourceSpecification {
        local_path,
        git_url,
        git_branch,
        git_tag,
        git_commit,
        registry_url,
    }
}

/// Determine the kind of the dependency to add.
fn determine_kind(matches: &ArgMatches) -> DependencyKind {
    if matches.get_flag("dev") {
        DependencyKind::Dev
    } else {
        DependencyKind::Normal
    }
}
