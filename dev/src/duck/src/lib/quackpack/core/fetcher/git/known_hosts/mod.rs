// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use git2::CertificateCheckStatus;
use git2::cert::Cert;

use crate::{DuckContext, QuackResult};

mod parse;

pub fn check_certificate(
    _ctx: &DuckContext,
    _certificate: &Cert<'_>,
    _host: &str,
) -> QuackResult<CertificateCheckStatus> {
    // @TODO: #3153 Implement
    Ok(CertificateCheckStatus::CertificatePassthrough)
}
