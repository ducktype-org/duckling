use std::{collections::HashMap, ffi::OsString, path::PathBuf};

use anyhow::bail;
use clap::ArgMatches;
use itertools::Itertools;
use quackpack::QuackResult;
use tracing::debug;

use crate::{
    DuckCtx,
    driver::{builtin::is_builtin_subcommand, cli, levenshtein, subcommands::subcommands},
};

pub fn fix_typos(
    args: ArgMatches,
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
) -> QuackResult<ArgMatches> {
    // No subcommand.
    let Some((name, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };

    if is_valid_subcmd(ctx, name, external_cmds)? {
        return Ok(args);
    }

    let targets = possible_targets(ctx, external_cmds)?;
    debug!(
        "Testing levenshtein of `{name}` against `{}`",
        targets.join(", ")
    );
    let closest_targets = find_closest_targets(name, &targets, ctx);
    print!("{:#?}", closest_targets);

    let Some((&first, rest)) = closest_targets.as_slice().split_first() else {
        return Ok(args);
    };

    if ctx.typos_fixes_enabled() && rest.is_empty() {
        let new_args = fix(name, first, subcmd_args)?;
        Ok(new_args)
    } else {
        bail!(make_levenshtein_nofix_msg(name, &closest_targets))
    }
}

fn is_valid_subcmd(
    ctx: &DuckCtx,
    name: &str,
    external_cmds: &HashMap<String, PathBuf>,
) -> QuackResult<bool> {
    Ok(is_builtin_subcommand(name)
        || ctx.alias_for(name)?.is_some()
        || external_cmds.contains_key(name))
}

fn possible_targets(
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
) -> QuackResult<Vec<String>> {
    let mut targets = subcommands()
        .into_iter()
        .map(|x| x.get_name().to_string())
        .collect::<Vec<_>>();
    targets.extend(ctx.aliases()?.keys().cloned());
    targets.extend(external_cmds.keys().cloned());
    Ok(targets)
}

fn find_closest_targets<'a>(bad_cmd: &str, targets: &'a [String], ctx: &DuckCtx) -> Vec<&'a str> {
    targets
        .iter()
        .filter_map(|target| {
            let distance = levenshtein::distance(target, bad_cmd);
            if distance <= ctx.max_fix_dist() {
                Some(target.as_str())
            } else {
                None
            }
        })
        .collect::<Vec<_>>()
}

fn make_levenshtein_nofix_msg(bad_cmd: &str, closest_targets: &[&str]) -> String {
    let suggestions = closest_targets
        .iter()
        .map(|target| format!("  - `{target}`"))
        .join("\n");
    format!("No such command as `{bad_cmd}`. Did you mean:\n{suggestions}?")
}

fn fix(bad_cmd: &str, new_subcmd: &str, subcmd_args: &ArgMatches) -> QuackResult<ArgMatches> {
    debug!("changing `{bad_cmd}` to `{new_subcmd}`");
    let new_cli_args = make_cli_args(new_subcmd, subcmd_args);
    parse_fixed_args(new_cli_args)
}

fn make_cli_args(fixed_cmd: &str, subcmd_args: &ArgMatches) -> Vec<OsString> {
    let mut result = vec![OsString::from(fixed_cmd)];
    result.extend(
        subcmd_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    );
    result
}

fn parse_fixed_args(new_cli_args: Vec<OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

#[cfg(test)]
mod tests {
    use super::*;
    use injectorpp::interface::injector::*;

    #[test]
    fn test_fixes() {
        let mut injector = InjectorPP::new();
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::aliases)(&DuckCtx) -> QuackResult<HashMap<String, String>>))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx) -> QuackResult<HashMap<String, String>>,
                returns: Ok(HashMap::new())
            ));
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::alias_for)(&DuckCtx, &str) -> QuackResult<Option<String>>))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx, _y: &str) -> QuackResult<Option<String>>,
                returns: Ok(None)
            ));
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::typos_fixes_enabled)(&DuckCtx) -> bool))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx) -> bool,
                returns: true
            ));
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::max_fix_dist)(&DuckCtx) -> u32))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx) -> u32,
                returns: 1
            ));

        let args_matches = cli().try_get_matches_from(["duck", "searcg"]).unwrap();
        let ctx = DuckCtx::new().unwrap();
        let external_cmds = HashMap::new();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert!(result.subcommand_name().unwrap() == "search")
    }
}
