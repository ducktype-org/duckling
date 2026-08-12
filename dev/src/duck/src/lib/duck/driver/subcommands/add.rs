use std::path::PathBuf;
use std::str::FromStr;

use crate::duck::driver::cli_ext::{multi, optional};
use crate::quackpack::core::Version;
use crate::quackpack::schemas::manifest::{DependencySource, DetailedSource};
use crate::quackpack::subcommands::add::{AddOptions, add};
use crate::{DuckContext, QuackResult};
use clap::{Arg, ArgGroup, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};

/// Creates parser for the `add` subcommand.
pub fn get_parser() -> Command {
    // Conflict are only defined top->down.
    subcommand("add")
        .about("Add a dependency to the current venv")
        .arg(flag("global", "Add a dependency from the global venv instead").short('g'))
        .arg(flag("dev", "Add a dev dependency instead"))
        .arg(flag("pinned", "Pin the dependency's version"))
        .arg(optional("alias", "How to alias the dependency"))
        .arg(optional("local", "Add a local dependency, specified by path")
            .conflicts_with("pinned")
        )
        .arg(optional("git-url", "Add a git dependency, on a repository under a URL")
            .conflicts_with("pinned")
        )
        .arg(optional("git-branch", "Specify a git branch of the dependency"))
        .arg(optional("git-tag", "Specify a git tag of the dependency"))
        .arg(optional("git-commit", "Specify a git commit of the dependency. Can be either short or long commit id"))
        .arg(optional("registry-url", "Specify a registry from which this dependency should be taken"))
        .group(ArgGroup::new("dependency-source").args(["local", "git-url", "registry-url"]))
        .group(ArgGroup::new("git-references").args(["git-branch", "git-tag", "git-commit"]).requires("git-url"))
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
    let name = matches.get_one::<String>("name").expect("guarded by the parser").clone();
    let source_name = if alias.is_some() { Some(name.clone()) } else { None };
    let source = if let Some(git_url) = matches.get_one::<String>("git-url") {
        let tag = matches.get_one("git-tag").cloned();
        let commit = matches.get_one("git-commit").cloned();
        let branch = matches.get_one("git-branch").cloned();
        Some(DependencySource::Detailed(DetailedSource { registry_url: None, name: source_name, path: None,
            git_url: Some(git_url.clone()), tag, commit, branch}))
    } else if let Some(registry_url) = matches.get_one::<String>("registry-url") {
        if source_name.is_some() {
            Some(DependencySource::Detailed(DetailedSource { registry_url: Some(registry_url.clone()), name: source_name,
            path: None, git_url: None, tag: None, commit: None, branch: None}))
        } else {
            Some(DependencySource::Simple(registry_url.clone()))
        }
    } else if let Some(path) = matches.get_one::<PathBuf>("local") {
        Some(DependencySource::Detailed(DetailedSource { registry_url: None, name: source_name, path: Some(path.clone()),
        git_url: None, tag: None, commit: None, branch: None}))
    } else {
        None
    };
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
    let options = AddOptions {
        name: matches
            .get_one::<String>("name")
            .expect("guarded by the parser")
            .into(),
        global: matches.get_flag("global"),
        dev_dep: matches.get_flag("dev"),
        pinned: matches.get_flag("pinned"),
        source,
        alias: matches.get_one::<String>("alias").map(Into::into),
        versions,
        features,
    };
    add(ctx, options)
}
