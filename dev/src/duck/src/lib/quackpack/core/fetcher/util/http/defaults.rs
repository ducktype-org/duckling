// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Default values for [`configure_easy2`](super::configure_easy2).
use std::time::Duration;

use http::HeaderValue;

// https://docs.rs/reqwest/latest/reqwest/struct.ClientBuilder.html#method.user_agent
/// Duck's user agent value.
pub const DUCK_USER_AGENT: &str = concat!(env!("CARGO_PKG_NAME"), "/", env!("CARGO_PKG_VERSION"),);

/// An arbitrary value.
pub const CONNECT_TIMEOUT: Duration = Duration::from_secs(60);
/// An arbitrary value.
pub const REQUEST_TIMEOUT: Duration = Duration::from_secs(60);

/// Maximal allowed number of redirects.
pub const MAX_REDIRECTS: u32 = 5;

/// A value for a valueless HTTP header.
///
/// Setting this value for a header HEADER results in `HEADER;` syntax (note the semicolon), which
/// gets interpreted by cURL to set an empty value for that header.
///
/// Setting `HEADER:` (note the colon and no value) from the cURL perspective means to disable this
/// header.
///
/// In particular, [`NO_VALUE`] does not mean “remove this header”, means “send a header with an
/// empty string”.
///
/// See also [cURL docs](https://docs.rs/curl/latest/curl/easy/struct.Easy2.html#method.http_headers).
pub const NO_VALUE: HeaderValue = HeaderValue::from_static("");
