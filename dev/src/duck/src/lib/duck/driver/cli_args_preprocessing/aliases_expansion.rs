use std::collections::HashMap;
use std::ffi::OsString;
use std::path::PathBuf;

use clap::ArgMatches;
use itertools::chain;
use serde::{Deserialize, de};
use tracing::{debug, trace};

use crate::duck::driver::cli;
use crate::duck::driver::cli_args_preprocessing::builtin::{
    get_builtin_alias_expansion, is_builtin_subcommand,
};
use crate::{DuckContext, QuackResult, qp_bail};

/// A single configuration alias.
/// Can be a string ("build --release"), or a list (["build", "--release"]).
#[derive(Debug)]
pub enum Alias {
    ToSplit(String),
    Splitted(Vec<String>),
}

impl Alias {
    pub fn split(&self) -> Vec<String> {
        match self {
            Self::ToSplit(joined) => joined.split(' ').map(String::from).collect(),
            Self::Splitted(splitted) => splitted.clone(),
        }
    }
}

impl<'de> de::Deserialize<'de> for Alias {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a string or a list of strings")
            .string(|alias| Ok(Self::ToSplit(alias.into())))
            .seq(|seq| seq.deserialize().map(Self::Splitted))
            .deserialize(deserializer)
    }
}

#[derive(Debug, Default, Deserialize)]
#[serde(transparent)]
pub struct Aliases(pub HashMap<String, Alias>);

/// Recursively replace current subcommand with an expanded user alias.
///
/// It supports expansions to the builtin subcommands, builtin aliases, and user defined (external)
/// subcommands.
///
/// Multiple aliases (aliases to aliases/aliases using other aliases) are supported too.
///
/// __NOTE:__ Global CLI flags may be lost during this process. You should extract them beforehand.
pub fn expand_aliases(
    args: ArgMatches,
    ctx: &DuckContext,
    external_cmds: &HashMap<String, PathBuf>,
    mut visited: Vec<String>,
) -> QuackResult<ArgMatches> {
    // User hasn't provided a subcommand, ignore...
    let Some((subcmd, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };
    let is_builtin = is_builtin_subcommand(subcmd);
    let alias = ctx.duck_cfg().alias_for(subcmd)?;
    let is_external = external_cmds.contains_key(subcmd);
    let builtin_alias = get_builtin_alias_expansion(subcmd);
    match (is_builtin, &alias, is_external, builtin_alias) {
        (false, None, true, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows an external subcommand"
            ))?;
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, Some(_), false, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows a user-defined alias"
            ))?;
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, Some(_), true, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows a user-defined alias and an external subcommand"
            ))?;
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, None, false, Some(builtin)) => expand_builtin_alias(builtin, subcmd_args),
        (true, Some(_), false, None) => {
            ctx.error_console().warning(format!(
                "builtin subcommand `{subcmd}` shadows a user-defined alias"
            ))?;
            Ok(args)
        }
        (false, Some(_), true, None) => {
            ctx.error_console().warning(format!(
                "external subcommand `{subcmd}` shadows a user-defined alias"
            ))?;
            Ok(args)
        }
        (false, Some(new), false, None) => {
            // This is the actually interesting part.
            let new_args = expand_single_alias(subcmd, subcmd_args, new, &mut visited)?;
            expand_aliases(new_args, ctx, external_cmds, visited)
        }
        _ => {
            debug!(
                %is_builtin,
                ?alias,
                %is_external,
                ?builtin_alias,
                "expanding aliases: default branch",
            );
            Ok(args)
        }
    }
}

/// Return a new [`ArgMatches`] after replacing the builtin alias with its subcommand.
///
/// `builtin` is __expanded__ builtin alias (for example, for alias `b` we expect `build` to be
/// passed as `builtin`), and `args` are parsed [`ArgMatches`] __with the builtin subcommand__.
#[tracing::instrument(skip_all)]
fn expand_builtin_alias(builtin: &str, args: &ArgMatches) -> QuackResult<ArgMatches> {
    debug!(%builtin);
    let builtin = OsString::from(builtin);
    Ok(cli().no_binary_name(true).try_get_matches_from(chain(
        [&builtin],
        args.get_many::<OsString>("").unwrap_or_default(),
    ))?)
}

/// Expand single user alias.
///
/// `alias` is alias we're expanding, `alias_args` are [`ArgMatches`] for that `alias`, `alias_expansion`
/// is expanded alias (taken from [`DuckContext`]), and `visited` is a vector of already expanded
/// aliases (in order to detect cycles).
#[tracing::instrument(skip_all, fields(%alias, ?alias_expansion))]
fn expand_single_alias(
    alias: &str,
    alias_args: &ArgMatches,
    alias_expansion: &Alias,
    visited: &mut Vec<String>,
) -> QuackResult<ArgMatches> {
    debug!(?visited);
    let new_cli_args = args_from_alias(alias_expansion, alias_args);
    let parsed = parse_alias_args(new_cli_args)?;
    let Some(new_subcmd) = parsed.subcommand_name() else {
        qp_bail!("user-defined alias `{alias}` does not have a subcommand")
    };
    visited.push(alias.into());
    check_alias_cycle(alias, new_subcmd, visited)?;
    Ok(parsed)
}

/// Get __all__ CLI args for the __expanded__ alias `alias`, and parent `subcmd_args`.
///
/// For example, for alias `foo = "bar -1"`, and command `duck foo a -b`, you should pass
/// `alias = "bar -1"`, and args for `foo a -b`, and get `"bar", "-1", "a", "-b"` in return.
fn args_from_alias(alias: &Alias, subcmd_args: &ArgMatches) -> impl Iterator<Item = OsString> {
    let split = alias.split().into_iter().map(OsString::from);
    chain(
        split,
        subcmd_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    )
}

/// Parse new cli args. They should __not__ include a binary name.
fn parse_alias_args(new_cli_args: impl Iterator<Item = OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

/// Check for an aliases cycle.
#[tracing::instrument]
fn check_alias_cycle(current: &str, next: &str, visited: &[String]) -> QuackResult<()> {
    trace!("checking alias cycle");
    if visited.contains(&next.into()) {
        qp_bail!(
            "user-defined alias `{current}` cycles: {} -> {next}",
            visited.join(" -> "),
        );
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use crate::DuckContext;
    use crate::duck::driver::cli;
    use crate::duck::driver::cli_args_preprocessing::aliases_expansion::expand_aliases;

    #[test]
    fn test_cycles() {
        let fake_aliases = HashMap::from([
            (String::from("x"), String::from("y --a")),
            (String::from("y"), String::from("z --b xd")),
            (String::from("z"), String::from("x")),
        ]);

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let mut ctx = DuckContext::new().unwrap();
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = HashMap::new();
        let visited = Vec::new();
        let err = expand_aliases(args_matches, &ctx, &external_cmds, visited).unwrap_err();
        assert_eq!(
            err.to_string(),
            "user-defined alias `z` cycles: x -> y -> z -> x"
        );
    }

    #[test]
    fn test_expands_ok() {
        let fake_aliases = HashMap::from([
            (String::from("x"), String::from("y")),
            (String::from("y"), String::from("z --all-features")),
            (String::from("z"), String::from("build")),
        ]);

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let mut ctx = DuckContext::new().unwrap();
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = HashMap::new();
        let visited = Vec::new();
        let new_args_matches = expand_aliases(args_matches, &ctx, &external_cmds, visited).unwrap();
        assert_eq!(new_args_matches.subcommand_name().unwrap(), "build");
    }
}
