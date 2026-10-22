// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Duck version helpers.

use std::fmt::Display;

use tracing::warn;

use crate::{DuckContext, QuackResult};

#[derive(Debug)]
pub struct Version {
    duck_version: &'static str,
    long_commit: &'static str,
    short_commit: &'static str,
    commit_date: &'static str,
    target: &'static str,
    host: &'static str,
    profile: &'static str,
    debug: bool,
    _opt_level: &'static str,
    git2: git2::Version,
    curl: curl::Version,
    openssl: &'static str,
}

impl Version {
    // NOTE: rust-analyzer gives false negatives here, with messages for `env!`.
    pub fn get() -> Self {
        // https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-crates
        let duck_version = env!("CARGO_PKG_VERSION", "this should be set by cargo");
        // These are set in build.rs.
        let long_commit = env!("DUCK_LONG_COMMIT", "this should be set in build.rs");
        let short_commit = env!("DUCK_SHORT_COMMIT", "this should be set in build.rs");
        let commit_date = env!("DUCK_COMMIT_DATE", "this should be set in build.rs");
        let target = env!("DUCK_TARGET", "this should be set in build.rs");
        let host = env!("DUCK_HOST", "this should be set in build.rs");
        let profile = env!("DUCK_PROFILE", "this should be set in build.rs");
        let is_debug = env!("DUCK_DEBUG", "this should be set in build.rs");
        let debug = match is_debug {
            "true" => true,
            "false" => false,
            _ => {
                warn!(?is_debug, "unknown debug value set by cargo");
                false
            }
        };

        let opt_level = env!("DUCK_OPT_LEVEL", "this should be set in build.rs");

        let git2 = git2::Version::get();
        let openssl = openssl::version::version();
        let curl = curl::Version::get();
        Self {
            duck_version,
            long_commit,
            short_commit,
            commit_date,
            target,
            host,
            profile,
            debug,
            _opt_level: opt_level,
            curl,
            git2,
            openssl,
        }
    }

    pub fn print(&self, ctx: &DuckContext, verbose: bool) -> QuackResult<()> {
        ctx.print(format!(
            "duck {} ({} {})",
            self.duck_version, self.short_commit, self.commit_date
        ))?;
        if !verbose {
            return Ok(());
        }
        print_field(ctx, "commit-hash", self.long_commit)?;
        print_field(ctx, "commit-date", self.commit_date)?;
        print_field(ctx, "build-type", self.profile)?;
        print_field(ctx, "is-debug", if self.debug { "yes" } else { "no" })?;
        print_field(ctx, "host", self.host)?;
        print_field(ctx, "target", self.target)?;
        let git2_string = {
            let vendored = if self.git2.vendored() {
                "vendored"
            } else {
                "system"
            };
            let v = self.git2.libgit2_version();
            format!("{}.{}.{} ({vendored})", v.0, v.1, v.2)
        };

        let curl_string = {
            let vendored = if self.curl.vendored() {
                "vendored"
            } else {
                "system"
            };
            format!("{} ({vendored})", self.curl.version())
        };
        print_field(ctx, "git", &git2_string)?;
        print_field(ctx, "curl", &curl_string)?;
        print_field(ctx, "ssl", self.openssl)?;
        Ok(())
    }
}

fn print_field(ctx: &DuckContext, field: &str, key: impl Display) -> QuackResult<()> {
    ctx.print(format!("{:<12} {}", format!("{field}:"), key))
}
