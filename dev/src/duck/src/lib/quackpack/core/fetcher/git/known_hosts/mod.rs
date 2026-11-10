// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Logic for [`RemoteCallbacks::certificate_check`].
//!
//! It performs parsing ([`parse`]), checking whether a line accepts a host ([`accept`]), and
//! checking common known hosts.
//!
//! [`RemoteCallbacks::certificate_check`]: git2::RemoteCallbacks::certificate_check

use std::path::{Path, PathBuf};
use std::{fmt, fs, io};

use git2::CertificateCheckStatus;
use git2::cert::Cert;
use tracing::{debug, error, info, trace, warn};

use self::parse::get_bundled_keys;
use crate::DuckContext;

mod accept;
mod parse;

#[tracing::instrument(level = "debug", skip_all)]
pub fn check_certificate(
    ctx: &DuckContext,
    certificate: &Cert<'_>,
    host: &str,
    port: Option<u16>,
) -> Result<CertificateCheckStatus, git2::Error> {
    debug!(target: "duck::git", ?host, ?port);
    let Some(host_cert) = certificate.as_hostkey() else {
        return Ok(CertificateCheckStatus::CertificatePassthrough);
    };
    let Some(hostkey) = host_cert.hostkey() else {
        return Ok(CertificateCheckStatus::CertificatePassthrough);
    };
    let Some(host_type) = host_cert.hostkey_type() else {
        warn!(target: "duck::git", "has hostkey, but not hostkey type");
        return Ok(CertificateCheckStatus::CertificatePassthrough);
    };
    // Nonstandard port might also be used. In that case, the host format is `[{host}]:{port}`.
    // 22 is the standard port.
    let host = match port {
        None | Some(22) => host.to_string(),
        Some(port) => format!("[{host}]:{port}"),
    };
    let files = get_known_hosts_files(ctx);
    debug!(target: "duck::git", ?files, ?host, ?hostkey, ?host_type);
    let entries = {
        let mut buffer = get_bundled_keys()
            .into_iter()
            .map(|entry| (entry, KnownHostSource::Bundled))
            .collect();
        for file in &files {
            parse_single_known_hosts_file(file, &mut buffer);
        }
        buffer
    };
    let mut encountered_accept = false;
    let mut encountered_revoked = false;
    for entry in entries {
        let (entry, path) = entry;
        let result = entry.accept(&host, hostkey, host_type);
        match result {
            accept::AcceptResult::Accepted => {
                info!(target: "duck::git", ?path, ?entry, "accepted certificate");
                encountered_accept = true;
            }
            accept::AcceptResult::Rejected => {
                trace!(target: "duck::git", ?path, ?entry, "rejected certificate");
            }
            accept::AcceptResult::Revoked => {
                warn!(target: "duck::git", ?path, ?entry, "revoked certificate");
                encountered_revoked = true;
            }
            accept::AcceptResult::NotMatched => {
                trace!(target: "duck::git", ?path, ?entry, "not matched");
            }
            accept::AcceptResult::KeyMismatch => {
                trace!(target: "duck::git", ?path, ?entry, "keys mismatch");
            }
            accept::AcceptResult::Unsupported => {
                trace!(target: "duck::git", ?path, ?entry, "unsupported key");
            }
        }
    }
    if encountered_revoked {
        let error = git2::Error::new(
            git2::ErrorCode::Certificate,
            git2::ErrorClass::Ssh,
            "tried to use a revoked key",
        );
        Err(error)
    } else if encountered_accept {
        Ok(CertificateCheckStatus::CertificateOk)
    } else {
        Ok(CertificateCheckStatus::CertificatePassthrough)
    }
}

#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single entry (line) in the `known_hosts` file.
struct KnownHostEntry {
    /// Marker of the entry.
    marker: Option<Marker>,
    /// List of patterns to match against.
    hosts: Vec<Hostname>,
    /// Keytype.
    key_type: String,
    /// Base64 decoded key.
    key: Vec<u8>,
}

impl fmt::Debug for KnownHostEntry {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("KnownHostEntry")
            .field("marker", &self.marker)
            .field("hosts", &self.hosts)
            .field("key_type", &self.key_type)
            .field("key", &"...")
            .finish()
    }
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single hostname pattern.
enum Hostname {
    Hashed(HashedHostname),
    Normal(NormalHostname),
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// “Normal” (patternable) single entry.
///
/// If the pattern is negated and it matches, _the entire line_ is discarded.
struct NormalHostname {
    /// Raw pattern.
    pattern: String,
    /// Whether the pattern is negated.
    is_negated: bool,
}

#[cfg_attr(test, derive(Eq, PartialEq))]
/// A hashed hostname entry.
///
/// Uses HMAC SHA1 algorithm.
struct HashedHostname {
    /// Salt for the hash. Already base64 decoded.
    salt: Vec<u8>,
    /// Base64 decoded host hash.
    host: Vec<u8>,
}

impl fmt::Debug for HashedHostname {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HashedHostname")
            .field("salt", &"...")
            .field("host", &"...")
            .finish()
    }
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single line in `known_hosts` file.
enum KnownHostsLine {
    /// This line is a comment.
    Comment,
    /// This line contains pattern to match against.
    Entry(KnownHostEntry),
    /// Failed to parse the line.
    Failed(parse::ParseError),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
/// Optional marker of the line.
enum Marker {
    /// This line is certification authority key.
    CA,
    /// This line contains a revoked key.
    Revoked,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
enum KnownHostSource<'a> {
    Path(&'a Path),
    Bundled,
}

type TrackedKnownHostEntry<'a> = (KnownHostEntry, KnownHostSource<'a>);

/// Get paths to the `known_hosts` files.
fn get_known_hosts_files(ctx: &DuckContext) -> Vec<PathBuf> {
    // Files are `/etc/ssh/ssh_known_hosts` and `~/.ssh/known_hosts`, though I suppose the former is
    // not supported on Windows.
    let mut system_wide_config = if cfg!(unix) {
        vec![PathBuf::from("/etc/ssh/ssh_known_hosts")]
    } else {
        vec![]
    };
    let user_config = ctx.user_home().join(".ssh").join("known_hosts");
    system_wide_config.push(user_config);
    system_wide_config
}

/// Parse a single known_hosts file, and append its results to `buffer`.
fn parse_single_known_hosts_file<'a>(path: &'a Path, buffer: &mut Vec<TrackedKnownHostEntry<'a>>) {
    let text = match fs::read_to_string(path) {
        Ok(text) => text,
        Err(io_err) => {
            if io_err.kind() == io::ErrorKind::NotFound {
                String::new()
            } else {
                error!(target: "duck::git", err = ?io_err, ?path, "failed to read known_hosts file, ignoring...");
                return;
            }
        }
    };
    let source = KnownHostSource::Path(path);
    let parsed = parse::parse_known_hosts_file(&text)
        .into_iter()
        .filter_map(|entry| match entry {
            KnownHostsLine::Comment => None,
            KnownHostsLine::Entry(known_host_entry) => Some((known_host_entry, source)),
            KnownHostsLine::Failed(err) => {
                error!(target: "duck::git", %err, ?path, "failed to parse a known_hosts line, ignoring...");
                None
            }
        });
    buffer.extend(parsed);
}
