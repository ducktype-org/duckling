use std::{collections::HashMap, ffi::OsString, mem::swap, path::PathBuf};

use anyhow::bail;
use clap::ArgMatches;
use itertools::Itertools;
use quackpack::QuackResult;
use tracing::debug;

use crate::{
    DuckCtx,
    driver::{builtin::is_builtin_subcommand, cli, subcommands::subcommands},
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
    // `name` is valid subcommand, ignore.
    if check_valid_subcmd(ctx, name, external_cmds)? {
        return Ok(args);
    };
    let targets = possible_targets(ctx, external_cmds)?;
    let closest_targets = find_closest_targets(name, &targets);
    let closest_targets_str = closest_targets.iter().map(|x| x.target).join(", ");

    debug!("Testing levenshtein of `{name}` against `{closest_targets_str}`.");
    debug!("Closest targets are: `{closest_targets_str}`.");

    if !ctx.typos_fixes_enabled()
        || closest_targets.len() > 1
        || fix_dist_exceeds_max(ctx, &closest_targets)
    {
        no_fix(name, &closest_targets)?;
        panic!("Unreachable");
    } else {
        let new_args = fix(name, &closest_targets, subcmd_args)?;
        Ok(new_args)
    }
}

fn check_valid_subcmd(
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

struct CorrectionCandidate<'a> {
    target: &'a str,
    distance: u32,
}

fn find_closest_targets<'a>(
    bad_cmd: &str,
    targets: &'a Vec<String>,
) -> Vec<CorrectionCandidate<'a>> {
    let mut results: Vec<CorrectionCandidate<'a>> = Vec::new();
    let mut cur_min: Option<u32> = None;
    for target in targets {
        let distance = levenshtein_distance(bad_cmd, target);
        let candidate = CorrectionCandidate { target, distance };
        if let Some(n) = cur_min {
            if distance < n {
                results = vec![candidate];
                cur_min = Some(n);
            } else if distance == n {
                results.push(candidate);
            }
        } else {
            results.push(CorrectionCandidate { target, distance });
            cur_min = Some(distance);
        }
    }
    results
}

fn levenshtein_distance(x: &str, y: &str) -> u32 {
    let word1: Vec<char> = x.chars().collect();
    let word2: Vec<char> = y.chars().collect();
    let m = x.len();
    let n = y.len();
    let mut v0 = Vec::from_iter(0..(n + 1));
    let mut v1 = vec![0, n + 1];
    for (i, w1_letter) in word1.iter().enumerate().take(m) {
        v1[0] = i + 1;
        for (j, w2_letter) in word2.iter().enumerate().take(n) {
            let deletion_cost = v0[j + 1] + 1;
            let insertion_cost = v1[j] + 1;
            let substitution_cost = v0[j] + (w1_letter != w2_letter) as usize;
            v1[j + 1] = *[deletion_cost, insertion_cost, substitution_cost]
                .iter()
                .min()
                .unwrap();
        }
        swap(&mut v0, &mut v1);
    }
    v0[n].try_into().unwrap()
}

fn fix_dist_exceeds_max(ctx: &DuckCtx, closest_targets: &[CorrectionCandidate]) -> bool {
    ctx.max_fix_dist() < closest_targets[0].distance
}

fn no_fix(bad_cmd: &str, closest_targets: &[CorrectionCandidate]) -> QuackResult<()> {
    let suggestions = suggestions_str(closest_targets);
    let error_msg = format!("No such command as `{bad_cmd}`. Did you mean `{suggestions}`?",);
    bail!(error_msg)
}

fn suggestions_str(closest_targets: &[CorrectionCandidate]) -> String {
    let n_targets = closest_targets.len();
    if closest_targets.len() == 1 {
        String::from(closest_targets[0].target)
    } else {
        format!(
            "{} or {}",
            closest_targets[0..n_targets - 1]
                .iter()
                .map(|x| x.target)
                .join(", "),
            closest_targets[n_targets - 1].target
        )
    }
}

fn fix(
    bad_cmd: &str,
    closest_targets: &[CorrectionCandidate],
    subcmd_args: &ArgMatches,
) -> QuackResult<ArgMatches> {
    let new_sub_cmd = closest_targets[0].target;
    debug!("Changed `{bad_cmd}` to `{new_sub_cmd}`");
    let new_cli_args = new_cli_args(new_sub_cmd, subcmd_args);
    Ok(new_arg_matches(new_cli_args)?)
}

fn new_cli_args(fixed_cmd: &str, subcmd_args: &ArgMatches) -> Vec<OsString> {
    let mut result = vec![OsString::from(fixed_cmd)];
    result.extend(
        subcmd_args
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
