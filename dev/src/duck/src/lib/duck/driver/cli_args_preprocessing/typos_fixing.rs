//! Try to (wisely) fix user typos.
use std::{collections::HashMap, ffi::OsString, path::PathBuf};

use crate::{DuckCtx, QuackResult, qp_bail};
use clap::ArgMatches;
use itertools::Itertools;
use tracing::debug;

use crate::duck::driver::{
    cli,
    cli_args_preprocessing::{
        builtin::{get_builtin_alias_expansion, get_builtin_aliases, is_builtin_subcommand},
        levenshtein,
    },
    subcommands::subcommands,
};

/// Try to fix user typos.
///
/// We're using [levenshtein distance](levenshtein::distance) for checking what is a typo.
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
    let closest_targets = find_closest_targets(name, &targets, ctx.duck_cfg().max_fix_dist()?);

    let Some((&first, rest)) = closest_targets.as_slice().split_first() else {
        return Ok(args);
    };

    if ctx.duck_cfg().fixes_enabled()? && rest.is_empty() {
        let new_args = fix(name, first, subcmd_args)?;
        Ok(new_args)
    } else {
        qp_bail!("{}", make_levenshtein_nofix_msg(name, &closest_targets))
    }
}

/// Check, if command `name` is a valid duck subcommand.
///
/// Valid subcommands are:
/// - builtin subcommands,
/// - builtin aliases,
/// - user-defined aliases,
/// - external subcommands.
fn is_valid_subcmd(
    ctx: &DuckCtx,
    name: &str,
    external_cmds: &HashMap<String, PathBuf>,
) -> QuackResult<bool> {
    Ok(is_builtin_subcommand(name)
        || get_builtin_alias_expansion(name).is_some()
        || ctx.duck_cfg().alias_for(name)?.is_some()
        || external_cmds.contains_key(name))
}

/// Get all known and valid subcommands.
///
/// This is a list containing all values for which [`is_valid_subcmd`] returns true.
fn possible_targets(
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
) -> QuackResult<Vec<String>> {
    let mut targets = subcommands()
        .into_iter()
        .map(|x| x.get_name().to_string())
        .collect::<Vec<_>>();
    if let Some(iter) = ctx.duck_cfg().aliases()? {
        targets.extend(iter.cloned());
    }
    targets.extend(external_cmds.keys().cloned());
    targets.extend(get_builtin_aliases().map(str::to_string));
    Ok(targets)
}

/// Get all closest targets to the `bad_cmd`, which are no further than `max_fix_dist` (in terms of
/// the Levenshtein distance).
fn find_closest_targets<'a>(
    bad_cmd: &str,
    targets: &'a [String],
    max_fix_dist: u32,
) -> Vec<&'a str> {
    targets
        .iter()
        .fold(Vec::new(), |acc, target| {
            update_closest_targets(acc, target, bad_cmd, max_fix_dist)
        })
        .into_iter()
        .map(|(target, _)| target)
        .collect()
}

/// Update closest targets given the actual stack, target, and `max_fix_dist`.
///
/// This function will __only__ keep those values, which are (currently) closest to the target,
/// and all have exactly the same distance.
fn update_closest_targets<'a>(
    mut acc: Vec<(&'a str, u32)>,
    target: &'a str,
    bad_cmd: &str,
    max_fix_dist: u32,
) -> Vec<(&'a str, u32)> {
    let dist = levenshtein::distance(bad_cmd, target);
    if dist > max_fix_dist {
        return acc;
    }
    match acc.len() {
        0 => {
            acc.push((target, dist));
        }
        _ => match (dist < acc[0].1, dist == acc[0].1) {
            (true, false) => {
                acc.clear();
                acc.push((target, dist));
            }
            (false, true) => {
                acc.push((target, dist));
            }
            _ => {}
        },
    }
    acc
}

/// Helper for creating a common message if we didn't fix a typo.
fn make_levenshtein_nofix_msg(bad_cmd: &str, closest_targets: &[&str]) -> String {
    let suggestions = closest_targets
        .iter()
        .map(|target| format!("- `{target}`"))
        .join("\n");
    format!("No such command as `{bad_cmd}`. Did you mean:\n{suggestions}?")
}

/// We'd guessed that `bad_cmd` can be replaced by `new_subcmd`.
///
/// Replace it and re-parse the arguments.
fn fix(bad_cmd: &str, new_subcmd: &str, subcmd_args: &ArgMatches) -> QuackResult<ArgMatches> {
    debug!("changing `{bad_cmd}` to `{new_subcmd}`");
    let new_cli_args = make_cli_args(new_subcmd, subcmd_args);
    parse_fixed_args(new_cli_args)
}

/// Create new CLI arguments for a fixed subcommand.
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

/// Re-parse fixed arguments.
fn parse_fixed_args(new_cli_args: Vec<OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fixes() {
        let args_matches = cli().try_get_matches_from(["duck", "buil"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_fixes_enabled(true);
        ctx.duck_cfg_mut().set_max_fix_dist(1);
        let external_cmds = HashMap::new();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("build"));
    }

    // !TODO: Reenable after enabling more subcommands.
    // #[test]
    // fn test_multiple_targets() {
    //     let args_matches = cli().try_get_matches_from(["duck", "inaa"]).unwrap();
    //     let mut ctx = DuckCtx::new().unwrap();
    //     ctx.duck_cfg_mut().set_fixes_enabled(true);
    //     ctx.duck_cfg_mut().set_max_fix_dist(100);
    //     let external_cmds = HashMap::new();
    //     let result = fix_typos(args_matches, &ctx, &external_cmds).expect_err(
    //         "There are two equally distant targets (`info` and `init`), so fixing should fail.",
    //     );
    //     assert_eq!(
    //         result.to_string(),
    //         "No such command as `inaa`. Did you mean:\n- `info`\n- `init`?"
    //     );
    // }

    #[test]
    fn test_single_closest_target() {
        let args_matches = cli().try_get_matches_from(["duck", "ini"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_fixes_enabled(true);
        ctx.duck_cfg_mut().set_max_fix_dist(100);
        let external_cmds = HashMap::new();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("init"));
    }

    #[test]
    fn test_fixes_to_alias() {
        let fake_aliases = HashMap::from([(
            String::from("my_alias"),
            String::from("expands to something -a --b c"),
        )]);

        let args_matches = cli().try_get_matches_from(["duck", "ny_aias"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_fixes_enabled(true);
        ctx.duck_cfg_mut().set_max_fix_dist(2);
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = HashMap::new();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("my_alias"));
    }

    #[test]
    fn test_fixes_to_external() {
        let args_matches = cli()
            .try_get_matches_from(["duck", "my_external_xmd"])
            .unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_fixes_enabled(true);
        ctx.duck_cfg_mut().set_max_fix_dist(1);
        let external_cmds = HashMap::from([(String::from("my_external_cmd"), PathBuf::new())]);
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("my_external_cmd"));
    }

    #[test]
    fn test_tries_fixing_to_builtin_alias() {
        let args_matches = cli().try_get_matches_from(["duck", "a"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_fixes_enabled(true);
        ctx.duck_cfg_mut().set_max_fix_dist(1);
        let external_cmds = HashMap::new();
        let result = fix_typos(args_matches, &ctx, &external_cmds).expect_err(
            "There are two equally distant targets (`b` and `r`), so fixing should fail.",
        );
        assert_eq!(
            result.to_string(),
            "No such command as `a`. Did you mean:\n- `b`\n- `r`?"
        );
    }
}
