use std::{collections::HashMap, ffi::OsString, path::PathBuf};

use crate::{
    DuckCtx,
    driver::{
        builtin::{get_builtin_alias, is_builtin_subcommand},
        cli,
    },
};
use anyhow::bail;
use clap::ArgMatches;
use quackpack::QuackResult;
use tracing::debug;

pub fn expand_aliases(
    args: ArgMatches,
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
    mut visited: Vec<String>,
) -> QuackResult<ArgMatches> {
    let Some((subcmd, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };
    debug!("expanding alias `{subcmd}`");
    match (
        is_builtin_subcommand(subcmd),
        ctx.alias_for(subcmd),
        external_cmds.contains_key(subcmd),
        get_builtin_alias(subcmd),
    ) {
        // TODO: This will be warned on, when inferring cmd.
        (_, Ok(None) | Err(_), _, _) => Ok(args),
        (true, Ok(Some(new)), false, None) => shadows_builtin_subcmd(ctx, &new, args),
        (false, Ok(Some(new)), true, None) => shadows_external_subcmd(ctx, &new, args),
        (false, Ok(Some(new)), false, Some(_)) => shadows_builtin_alias(ctx, &new, args),
        (false, Ok(Some(new)), false, None) => {
            // This is the actually interesting part.
            let new_args = expand_single_alias(&new, subcmd, subcmd_args, &mut visited)?;
            expand_aliases(new_args, ctx, external_cmds, visited)
        }
        _ => Ok(args),
    }
}

fn shadows_builtin_subcmd(ctx: &DuckCtx, alias: &str, args: ArgMatches) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows builtin subcommand, ignoring it..."
    ));
    Ok(args)
}

fn shadows_external_subcmd(
    ctx: &DuckCtx,
    alias: &str,
    args: ArgMatches,
) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows external subcommand, ignoring it..."
    ));
    Ok(args)
}

fn shadows_builtin_alias(ctx: &DuckCtx, alias: &str, args: ArgMatches) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows builtin alias, ignoring it..."
    ));
    Ok(args)
}

fn expand_single_alias(
    alias: &str,
    subcmd: &str,
    prev_subcmd_matches: &ArgMatches,
    visited: &mut Vec<String>,
) -> QuackResult<ArgMatches> {
    let new_cli_args = new_cli_args(alias, prev_subcmd_matches);
    debug!("replaced alias `{subcmd}` with `{new_cli_args:?}`");
    let new_subcmd_matches = new_arg_matches(new_cli_args)?;
    let new_subcmd = try_get_new_subcmd(&new_subcmd_matches, alias)?;
    check_no_cycle(subcmd, new_subcmd, visited, alias)?;
    Ok(new_subcmd_matches)
}

fn new_cli_args(alias: &str, prev_subcmd_matches: &ArgMatches) -> Vec<OsString> {
    let mut result = alias.split(' ').map(OsString::from).collect::<Vec<_>>();
    result.extend(
        prev_subcmd_matches
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    );
    result
}

fn new_arg_matches(new_cli_args: Vec<OsString>) -> Result<ArgMatches, clap::Error> {
    cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)
}

fn try_get_new_subcmd<'a>(parsed: &'a ArgMatches, alias: &str) -> QuackResult<&'a str> {
    let Some(new_subcmd) = parsed.subcommand_name() else {
        bail!("user-defined alias `{alias}` does not have subcommand")
    };
    Ok(new_subcmd)
}

fn check_no_cycle(
    subcmd: &str,
    new_subcmd: &str,
    visited: &mut Vec<String>,
    alias: &str,
) -> QuackResult<()> {
    visited.push(subcmd.into());
    if visited.contains(&new_subcmd.into()) {
        bail!(
            "user-defined alias `{alias}` cycles: {} -> {}",
            visited.join(" -> "),
            alias
        );
    }
    Ok(())
}
