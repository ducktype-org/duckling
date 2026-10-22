// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for getting the version of duckc.

use std::sync::OnceLock;

use tracing::error;

use super::Duckc;
use crate::util::hash::sha256_string;
use crate::{DuckContext, QuackResult, qp_bail_internal};

/// Struct containing all information about duckc and its dependencies versions.
#[derive(Debug, Clone)]
pub struct DuckcVersion {
    /// Raw output of `duckc --version-verbose`.
    raw_output: String,
}

impl DuckcVersion {
    /// Create a new [`DuckcVersion`].
    /// Private by design, so that the version can be only determined by [`Duckc::get_version`].
    fn new(raw_output: String) -> Self {
        Self { raw_output }
    }

    /// Sha256 hash of the [`DuckcVersion`].
    pub fn hash_sha256(&self) -> String {
        sha256_string(&self.raw_output)
    }
}

/// The [`DuckcVersion`] for this run of duck.
static DUCKC_VERSION: OnceLock<DuckcVersion> = OnceLock::new();

impl Duckc {
    pub fn get_version(ctx: &DuckContext) -> QuackResult<&'static DuckcVersion> {
        if let Some(version) = DUCKC_VERSION.get() {
            return Ok(version);
        }
        let stdout = Self::get_version_impl(ctx)?;
        Ok(DUCKC_VERSION.get_or_init(|| DuckcVersion::new(stdout)))
    }

    /// Helper for the verbose version of `duckc` (the result of `duckc --version-verbose`).
    fn get_version_impl(ctx: &DuckContext) -> QuackResult<String> {
        let this = Self::new(ctx);
        let output = this.process_builder().ask_for_verbose_version().output()?;
        let status = output.status;
        if !status.success() {
            error!(%status, "invoking duckc failed, status is not success");
            // We captured stderr, so print it with the error.
            qp_bail_internal!("failed to run `duckc --version-verbose`: {output:?}")
        }
        Ok(String::from_utf8(output.stdout)
            .expect("`duckc --version-verbose` returned invalid utf8"))
    }
}
