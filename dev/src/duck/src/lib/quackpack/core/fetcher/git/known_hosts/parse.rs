// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Parse a `known_hosts` file.
//! All sections are copied from: <https://man.openbsd.org/sshd#SSH_KNOWN_HOSTS_FILE_FORMAT>.
//! Last access time: 1st of November 2026.

#![expect(dead_code)]

use base64::prelude::*;
use tracing::{debug, trace};

/// Marker indicating a certification authority (CA) key.
const CA_MARKER: &str = "@cert-authority";
/// Marker indicating a revoked key.
const REVOKED_MARKER: &str = "@revoked";

// https://github.com/openssh/openssh-portable/blob/1fbdcb402516aacce59b4cc7b7d98f06820a1e44/hostfile.h#L59-L60
/// From libssh internals: the only supported hash algorithm is HMAC SHA1, and libssh checks that
/// the hostname actually starts with it.
const HASH_MAGIC: &str = "|1|";
/// From libssh internals: delimiter used for distinguishing between different parts of hashes.
const HASH_DELIMITER: char = '|';
/// Hashed hostnames start with a pipe.
const HASH_HOSTNAME_START: char = '|';

// Hostnames is a comma-separated list of patterns...
/// Delimiter used to allow multiple patterns on a single line.
const HOSTNAMES_DELIMITER: char = ',';

// A pattern may also be preceded by ‘!’ to indicate negation...
/// If a pattern starts with this symbol, it's negated.
const NEGATED_HOSTNAME: char = '!';

/// If a line starts with this symbol, it's a comment.
const COMMENT_CHAR: char = '#';

pub type ParseResult<T> = Result<T, ParseError>;

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Error type indicating failed parsing.
pub enum ParseError {
    /// Obligatory field was missing.
    MissingField(MissingFieldType),
    /// Hashed hostname was missing magic.
    MissingHashMagic,
    /// Hashed hostname was missing salt or host fields.
    MissingHashSaltOrHost,
    /// Hashed hostname contains too many delimiters.
    TooManyHashDelims,
    /// An error occurred while performing a base64 decoding.
    Base64Decoding(base64::DecodeError),
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Which obligatory field was missing.
pub enum MissingFieldType {
    /// Hostnames (patterns)
    Hostnames,
    /// Keytype
    Keytype,
    /// Key
    Key,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single line in `known_hosts` file.
pub enum KnownHostsLine {
    /// This line is a comment.
    Comment,
    /// This line contains pattern to match against.
    Entry(KnownHostEntry),
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Optional marker of the line.
pub enum Marker {
    /// This line is certification authority key.
    CA,
    /// This line contains a revoked key.
    Revoked,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// “Normal” (patternable) single entry.
///
/// If the pattern is negated and it matches, _the entire line_ is discarded.
pub struct NormalHostname {
    /// Raw pattern.
    pattern: String,
    /// Whether the pattern is negated.
    is_negated: bool,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A hashed hostname entry.
///
/// Uses HMAC SHA1 algorithm.
pub struct HashedHostname {
    /// Salt for the hash. Already base64 decoded.
    salt: Vec<u8>,
    /// Base64 decoded host hash.
    host: Vec<u8>,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single hostname pattern.
pub enum Hostname {
    Hashed(HashedHostname),
    Normal(NormalHostname),
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// A single entry (line) in the `known_hosts` file.
pub struct KnownHostEntry {
    /// Marker of the entry.
    marker: Option<Marker>,
    /// List of patterns to match against.
    hosts: Vec<Hostname>,
    /// Keytype.
    key_type: String,
    /// Base64 decoded key.
    key: Vec<u8>,
}

/// Parse a `known_hosts` file.
pub fn parse_known_hosts_file(file: &str) -> ParseResult<Vec<KnownHostsLine>> {
    file.lines().map(parse_known_hosts_line).collect()
}

/// Parse a single `known_hosts` line.
pub fn parse_known_hosts_line(line: &str) -> ParseResult<KnownHostsLine> {
    let line = line.trim_ascii();
    trace!(target: "duck::git", ?line, "parsing line");
    // Lines starting with ‘#’ and empty lines are ignored as comments.
    if line.is_empty() || line.starts_with(COMMENT_CHAR) {
        return Ok(KnownHostsLine::Comment);
    }
    // Each line in these files contains the following fields: marker (optional), hostnames, keytype,
    // base64-encoded key, comment. The fields are separated by spaces.
    let mut iter = line.split_ascii_whitespace();
    let marker_or_hostnames = iter
        .next()
        .expect("line was trimmed and is nonempty, should yield at least one element");

    // Marker is optional.
    let marker = match marker_or_hostnames {
        CA_MARKER => Some(Marker::CA),
        REVOKED_MARKER => Some(Marker::Revoked),
        _ => None,
    };

    trace!(target: "duck::git", ?marker, "parsed marker");

    let hostnames_str = if marker.is_some() {
        // Advance iter to get hostnames.
        iter.next()
            .ok_or(ParseError::MissingField(MissingFieldType::Hostnames))?
    } else {
        // No marker, that were hostnames.
        marker_or_hostnames
    };
    let hosts = parse_hostnames(hostnames_str)?;
    let key_type = iter
        .next()
        .ok_or(ParseError::MissingField(MissingFieldType::Keytype))?;
    let base64_key = iter
        .next()
        .ok_or(ParseError::MissingField(MissingFieldType::Key))?;
    let key = BASE64_STANDARD
        .decode(base64_key)
        .map_err(ParseError::Base64Decoding)?;
    let entry = KnownHostEntry {
        marker,
        hosts,
        key_type: key_type.to_string(),
        key,
    };
    trace!(target: "duck::git", ?entry, "parsed known_hosts entry");
    Ok(KnownHostsLine::Entry(entry))
}

/// Parse hostnames pattern(s).
/// There are two types of patterns:
/// * hashed: they start with a pipe. They are handled by [`parse_hashed_hostname`].
/// * _other_: then it is a comma-separated list of patterns. Each pattern can start with an
///   exclamation mark. Hostnames can also contain widlcards, but for parsing we ignore them.
fn parse_hostnames(hostnames: &str) -> ParseResult<Vec<Hostname>> {
    // This is a hashed hostname.
    if hostnames.starts_with(HASH_HOSTNAME_START) {
        debug!(target: "duck::git", "detected hashed hostname");
        let hashed = parse_hashed_hostname(hostnames)?;
        trace!(target: "duck::git", ?hashed, "parsed hashed hostname");
        return Ok(vec![Hostname::Hashed(hashed)]);
    }
    let mut parsed = vec![];
    // Hostnames is a comma-separated list of patterns...
    for hostname in hostnames.split(HOSTNAMES_DELIMITER) {
        // If a pattern starts with an exclamation mark, it is negated.
        let (is_negated, hostname) = match hostname.strip_prefix(NEGATED_HOSTNAME) {
            Some(stripped) => (true, stripped),
            None => (false, hostname),
        };
        let host = NormalHostname {
            pattern: hostname.to_string(),
            is_negated,
        };
        trace!(target: "duck::git", ?host, "parsed normal host");
        parsed.push(Hostname::Normal(host));
    }
    Ok(parsed)
}

/// SSHD docs only mention that hashed hostnames:
/// * start with a pipe,
/// * there can be only one hashed hostname in a line,
/// * it cannot be negated nor can it contain wildcards.
///
/// However, we somehow need to decode the hostname. Digging into libssh internals we can find out that:
/// * the only supported hashing algorithm is HMAC SHA1,
/// * actual (internal format) is `|1|<base64 encoded salt>|<base64 encoded host>`.
///
/// So we don't bother checking for negations, wildcards nor multiple hosts: in the worst case
/// scenario base64 decoding would fail, or we will misinterpret the line.
fn parse_hashed_hostname(hostname: &str) -> ParseResult<HashedHostname> {
    debug_assert!(
        hostname.starts_with(HASH_DELIMITER),
        "should be checked by the caller"
    );
    let Some(hostname) = hostname.strip_prefix(HASH_MAGIC) else {
        return Err(ParseError::MissingHashMagic);
    };
    let Some((salt, host)) = hostname.split_once(HASH_DELIMITER) else {
        return Err(ParseError::MissingHashSaltOrHost);
    };
    if host.contains(HASH_DELIMITER) {
        return Err(ParseError::TooManyHashDelims);
    }
    let salt = BASE64_STANDARD
        .decode(salt)
        .map_err(ParseError::Base64Decoding)?;
    let host = BASE64_STANDARD
        .decode(host)
        .map_err(ParseError::Base64Decoding)?;
    Ok(HashedHostname { salt, host })
}

#[cfg(test)]
mod tests {
    use super::*;

    const KEY_TYPE: &str = "ssh-ed25519";
    // Duckling in base64.
    const KEY: &str = "RHVja2xpbmc=";
    const DUCKLING: &[u8] = b"Duckling";

    fn normal(name: &str, is_negated: bool) -> Hostname {
        Hostname::Normal(NormalHostname {
            pattern: name.to_string(),
            is_negated,
        })
    }

    fn hashed(salt: &[u8], host: &[u8]) -> Hostname {
        Hostname::Hashed(HashedHostname {
            salt: salt.into(),
            host: host.into(),
        })
    }

    fn entry(
        marker: Option<Marker>,
        hosts: Vec<Hostname>,
        key_type: &str,
        key: &[u8],
    ) -> KnownHostsLine {
        KnownHostsLine::Entry(KnownHostEntry {
            marker,
            hosts,
            key_type: key_type.to_string(),
            key: key.into(),
        })
    }

    #[test]
    fn comments() {
        for line in [
            "",
            " ",
            "\t",
            "   \t  ",
            "# comment",
            "  # comment",
            "\t# comment",
            "#",
            "   # comment with spaces   ",
        ] {
            assert_eq!(parse_known_hosts_line(line), Ok(KnownHostsLine::Comment));
        }
    }

    #[test]
    fn basic_host() {
        assert_eq!(
            parse_known_hosts_line(&format!("example.com {KEY_TYPE} {KEY}")),
            Ok(entry(
                None,
                vec![normal("example.com", false)],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn multiple_hosts() {
        assert_eq!(
            parse_known_hosts_line(&format!(
                "example.com,server.example.com,192.168.1.1 {KEY_TYPE} {KEY}"
            )),
            Ok(entry(
                None,
                vec![
                    normal("example.com", false),
                    normal("server.example.com", false),
                    normal("192.168.1.1", false),
                ],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn hosts_and_wildcards() {
        assert_eq!(
            parse_known_hosts_line(&format!(
                "*.example.com,server?,host-*,?erver.example.com {KEY_TYPE} {KEY}"
            )),
            Ok(entry(
                None,
                vec![
                    normal("*.example.com", false),
                    normal("server?", false),
                    normal("host-*", false),
                    normal("?erver.example.com", false),
                ],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn negated_hosts() {
        assert_eq!(
            parse_known_hosts_line(&format!(
                "example.com,!blocked.example.com,!192.168.1.1 {KEY_TYPE} {KEY}"
            )),
            Ok(entry(
                None,
                vec![
                    normal("example.com", false),
                    normal("blocked.example.com", true),
                    normal("192.168.1.1", true),
                ],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn ca_marker() {
        assert_eq!(
            parse_known_hosts_line(&format!("@cert-authority *.example.com {KEY_TYPE} {KEY}")),
            Ok(entry(
                Some(Marker::CA),
                vec![normal("*.example.com", false)],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn revoked_marker() {
        assert_eq!(
            parse_known_hosts_line(&format!("@revoked example.com {KEY_TYPE} {KEY}")),
            Ok(entry(
                Some(Marker::Revoked),
                vec![normal("example.com", false)],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn trailing_comments_are_fine() {
        assert_eq!(
            parse_known_hosts_line(&format!("example.com {KEY_TYPE} {KEY} some comment")),
            Ok(entry(
                None,
                vec![normal("example.com", false)],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn ignores_leading_and_trailing_whitespace() {
        assert_eq!(
            parse_known_hosts_line(&format!("  \t example.com {KEY_TYPE} {KEY}  \t ")),
            Ok(entry(
                None,
                vec![normal("example.com", false)],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn hashed_hostname() {
        let salt_base64 = "c2FsdA==";
        let host_base64 = "aG9zdA==";
        assert_eq!(
            parse_known_hosts_line(&format!("|1|{salt_base64}|{host_base64} {KEY_TYPE} {KEY}")),
            Ok(entry(
                None,
                vec![hashed(b"salt", b"host")],
                KEY_TYPE,
                DUCKLING,
            ))
        );
    }

    #[test]
    fn parse_file() {
        let file = format!(
            "\
# comment

example.com {KEY_TYPE} {KEY}
@cert-authority *.example.com {KEY_TYPE} {KEY}
@revoked revoked.example.com {KEY_TYPE} {KEY}
"
        );

        assert_eq!(
            parse_known_hosts_file(&file),
            Ok(vec![
                KnownHostsLine::Comment,
                KnownHostsLine::Comment,
                entry(None, vec![normal("example.com", false)], KEY_TYPE, DUCKLING),
                entry(
                    Some(Marker::CA),
                    vec![normal("*.example.com", false)],
                    KEY_TYPE,
                    DUCKLING,
                ),
                entry(
                    Some(Marker::Revoked),
                    vec![normal("revoked.example.com", false)],
                    KEY_TYPE,
                    DUCKLING,
                ),
            ])
        );
    }

    #[test]
    fn empty_file() {
        assert_eq!(parse_known_hosts_file(""), Ok(vec![]));
    }

    #[test]
    fn missing_hostnames_error() {
        for line in ["@cert-authority", "@revoked"] {
            assert_eq!(
                parse_known_hosts_line(line),
                Err(ParseError::MissingField(MissingFieldType::Hostnames))
            );
        }
    }

    #[test]
    fn missing_keytype_error() {
        assert_eq!(
            parse_known_hosts_line("example.com"),
            Err(ParseError::MissingField(MissingFieldType::Keytype))
        );
    }

    #[test]
    fn missing_key_error() {
        assert_eq!(
            parse_known_hosts_line("example.com ssh-ed25519"),
            Err(ParseError::MissingField(MissingFieldType::Key))
        );
    }

    #[test]
    fn missing_hash_magic_error() {
        assert_eq!(
            parse_known_hosts_line(&format!("|2|some-salt|some-host {KEY_TYPE} {KEY}")),
            Err(ParseError::MissingHashMagic)
        );
    }

    #[test]
    fn missing_hash_salt_or_host_error() {
        assert_eq!(
            parse_known_hosts_line(&format!("|1|some-salt {KEY_TYPE} {KEY}")),
            Err(ParseError::MissingHashSaltOrHost)
        );
    }

    #[test]
    fn too_many_hash_delimiters_error() {
        assert_eq!(
            parse_known_hosts_line(&format!("|1|some-salt|some-host|extra {KEY_TYPE} {KEY}")),
            Err(ParseError::TooManyHashDelims)
        );
    }

    #[test]
    fn bsd_docs() {
        let salt_base64 = "c2FsdA==";
        let host_base64 = "aG9zdA==";
        let file = format!(
            "\
# Comments allowed at start of line
cvs.example.net,192.0.2.10 {KEY_TYPE} {KEY}
# A hashed hostname
|1|{salt_base64}|{host_base64} {KEY_TYPE} {KEY}
# A revoked key
@revoked * {KEY_TYPE} {KEY}
# A CA key, accepted for any host in *.mydomain.com or *.mydomain.org
@cert-authority *.mydomain.org,*.mydomain.com {KEY_TYPE} {KEY}
"
        );

        assert_eq!(
            parse_known_hosts_file(&file),
            Ok(vec![
                KnownHostsLine::Comment,
                entry(
                    None,
                    vec![
                        normal("cvs.example.net", false),
                        normal("192.0.2.10", false)
                    ],
                    KEY_TYPE,
                    DUCKLING,
                ),
                KnownHostsLine::Comment,
                entry(None, vec![hashed(b"salt", b"host")], KEY_TYPE, DUCKLING),
                KnownHostsLine::Comment,
                entry(
                    Some(Marker::Revoked),
                    vec![normal("*", false)],
                    KEY_TYPE,
                    DUCKLING,
                ),
                KnownHostsLine::Comment,
                entry(
                    Some(Marker::CA),
                    vec![
                        normal("*.mydomain.org", false),
                        normal("*.mydomain.com", false)
                    ],
                    KEY_TYPE,
                    DUCKLING,
                ),
            ])
        );
    }
}
