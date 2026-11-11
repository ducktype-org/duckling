// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Parse a `known_hosts` file.
//! All sections are copied from: <https://man.openbsd.org/sshd#SSH_KNOWN_HOSTS_FILE_FORMAT>.
//! Last access time: 1st of November 2026.

use std::fmt;

use base64::prelude::*;
use thiserror::Error;
use tracing::{debug, error, trace};

use super::{HashedHostname, Hostname, KnownHostEntry, KnownHostsLine, Marker, NormalHostname};

/// A bundled key wrapper.
#[derive(Debug)]
struct BundledKey {
    revoked: bool,
    host: &'static str,
    keytype: &'static str,
    key: &'static str,
}

/// Known bundled keys. Resources:
/// * github:
///   <https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints>,
///   last access: 10-09-2026.
/// * gitlab:
///   <https://docs.gitlab.com/user/gitlab_com#ssh-known_hosts-entries>
///   last access: 10-09-2026.
///
/// Format is `(host, keytype, key)`.
const BUNDLED_KEYS: &[BundledKey] = &[
    BundledKey {
        revoked: false,
        host: "github.com",
        keytype: "ssh-ed25519",
        key: "AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl",
    },
    BundledKey {
        revoked: false,
        host: "github.com",
        keytype: "ecdsa-sha2-nistp256",
        key: "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBEmKSENjQEezOmxkZMy7opKgwFB9nkt5YRrYMjNuG5N87uRgg6CLrbo5wAdT/y6v0mKV0U2w0WZ2YB/++Tpockg=",
    },
    BundledKey {
        revoked: false,
        host: "github.com",
        keytype: "ssh-rsa",
        key: "AAAAB3NzaC1yc2EAAAADAQABAAABgQCj7ndNxQowgcQnjshcLrqPEiiphnt+VTTvDP6mHBL9j1aNUkY4Ue1gvwnGLVlOhGeYrnZaMgRK6+PKCUXaDbC7qtbW8gIkhL7aGCsOr/C56SJMy/BCZfxd1nWzAOxSDPgVsmerOBYfNqltV9/hWCqBywINIR+5dIg6JTJ72pcEpEjcYgXkE2YEFXV1JHnsKgbLWNlhScqb2UmyRkQyytRLtL+38TGxkxCflmO+5Z8CSSNY7GidjMIZ7Q4zMjA2n1nGrlTDkzwDCsw+wqFPGQA179cnfGWOWRVruj16z6XyvxvjJwbz0wQZ75XK5tKSb7FNyeIEs4TT4jk+S4dhPeAUC5y+bDYirYgM4GC7uEnztnZyaVWQ7B381AK4Qdrwt51ZqExKbQpTUNn+EjqoTwvqNj4kqx5QUCI0ThS/YkOxJCXmPUWZbhjpCg56i+2aB6CmK2JGhn57K5mj0MNdBXA4/WnwH6XoPWJzK5Nyu2zB3nAZp+S5hpQs+p1vN1/wsjk=",
    },
    BundledKey {
        revoked: false,
        host: "gitlab.com",
        keytype: "ssh-ed25519",
        key: "AAAAC3NzaC1lZDI1NTE5AAAAIAfuCHKVTjquxvt6CM6tdG4SLp1Btn/nOeHHE5UOzRdf",
    },
    BundledKey {
        revoked: false,
        host: "gitlab.com",
        keytype: "ssh-rsa",
        key: "AAAAB3NzaC1yc2EAAAADAQABAAABAQCsj2bNKTBSpIYDEGk9KxsGh3mySTRgMtXL583qmBpzeQ+jqCMRgBqB98u3z++J1sKlXHWfM9dyhSevkMwSbhoR8XIq/U0tCNyokEi/ueaBMCvbcTHhO7FcwzY92WK4Yt0aGROY5qX2UKSeOvuP4D6TPqKF1onrSzH9bx9XUf2lEdWT/ia1NEKjunUqu1xOB/StKDHMoX4/OKyIzuS0q/T1zOATthvasJFoPrAjkohTyaDUz2LN5JoH839hViyEG82yB+MjcFV5MU3N1l1QL3cVUCh93xSaua1N85qivl+siMkPGbO5xR/En4iEY6K2XPASUEMaieWVNTRCtJ4S8H+9",
    },
    BundledKey {
        revoked: false,
        host: "gitlab.com",
        keytype: "ecdsa-sha2-nistp256",
        key: "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBFSMqzJeV9rUzU4kWitGjeR4PWSa29SPqJ1fVkhtj3Hw9xjLVXVYrU9QlYWrOLXBpQ6KWjbjTDTdDkoohFzgbEY=",
    },
];

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

#[derive(Debug, Error)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Error type indicating failed parsing.
pub enum ParseError {
    /// Obligatory field was missing.
    #[error("missing obligatory field `{0}`")]
    MissingField(KnownHostField),
    /// Hashed hostname was missing magic.
    #[error("hashed hostname is missing the magic identifier")]
    MissingHashMagic,
    /// Hashed hostname was missing salt or host fields.
    #[error("hashed hostname is missing a salt or a host")]
    MissingHashSaltOrHost,
    /// Hashed hostname contains too many delimiters.
    #[error("hashed hostname contains too many delimiters")]
    TooManyHashDelims,
    /// An error occurred while performing a base64 decoding.
    #[error("failed to base64 decode field `{0}`: {1}")]
    Base64Decoding(KnownHostField, base64::DecodeError),
}

#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// All known hosts fields
pub enum KnownHostField {
    /// Salt of hashed hostname
    HashedSalt,
    /// Host of hashed hostname
    HashedHost,
    /// Hostnames (patterns)
    Hostnames,
    /// Keytype
    Keytype,
    /// Key
    Key,
}

impl fmt::Display for KnownHostField {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::HashedSalt => "hashed salt",
            Self::HashedHost => "hashed host",
            Self::Hostnames => "hostnames' patterns",
            Self::Keytype => "key-type",
            Self::Key => "key",
        };
        f.write_str(name)
    }
}

/// Parse a `known_hosts` file.
pub fn parse_known_hosts_file(file: &str) -> Vec<KnownHostsLine> {
    file.lines()
        .map(|line| match parse_known_hosts_line(line) {
            Ok(line) => line,
            Err(err) => KnownHostsLine::Failed(err),
        })
        .collect()
}

/// Get bundled known hosts keys.
pub fn get_bundled_keys() -> Vec<KnownHostEntry> {
    BUNDLED_KEYS
        .iter()
        .map(
            |BundledKey {
                 revoked,
                 host,
                 keytype,
                 key,
             }| {
                let revoked = if *revoked {
                    format!("{} ", REVOKED_MARKER)
                } else {
                    String::new()
                };
                let line = format!("{revoked}{host} {keytype} {key}");
                match parse_known_hosts_line(&line) {
                    Ok(KnownHostsLine::Entry(entry)) => entry,
                    other => unreachable!("bundled keys are valid, but got: {other:?}"),
                }
            },
        )
        .collect()
}

/// Parse a single `known_hosts` line.
fn parse_known_hosts_line(line: &str) -> ParseResult<KnownHostsLine> {
    let line = line.trim_ascii();
    trace!(?line, "parsing line");
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

    trace!(?marker, "parsed marker");

    let hostnames_str = if marker.is_some() {
        // Advance iter to get hostnames.
        iter.next()
            .ok_or(ParseError::MissingField(KnownHostField::Hostnames))?
    } else {
        // No marker, that were hostnames.
        marker_or_hostnames
    };
    let hosts = parse_hostnames(hostnames_str)?;
    let key_type = iter
        .next()
        .ok_or(ParseError::MissingField(KnownHostField::Keytype))?;
    let base64_key = iter
        .next()
        .ok_or(ParseError::MissingField(KnownHostField::Key))?;
    let key = BASE64_STANDARD
        .decode(base64_key)
        .map_err(|err| ParseError::Base64Decoding(KnownHostField::Key, err))?;
    let entry = KnownHostEntry {
        marker,
        hosts,
        key_type: key_type.to_string(),
        key,
    };
    trace!(?entry, "parsed known_hosts entry");
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
        debug!("detected hashed hostname");
        let hashed = parse_hashed_hostname(hostnames)?;
        trace!(?hashed, "parsed hashed hostname");
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
        trace!(?host, "parsed normal host");
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
        .map_err(|err| ParseError::Base64Decoding(KnownHostField::HashedSalt, err))?;
    let host = BASE64_STANDARD
        .decode(host)
        .map_err(|err| ParseError::Base64Decoding(KnownHostField::HashedHost, err))?;
    Ok(HashedHostname { salt, host })
}

#[cfg(test)]
mod tests {
    use git2::cert::SshHostKeyType;

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
            vec![
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
            ]
        );
    }

    #[test]
    fn empty_file() {
        assert_eq!(parse_known_hosts_file(""), vec![]);
    }

    #[test]
    fn missing_hostnames_error() {
        for line in ["@cert-authority", "@revoked"] {
            assert_eq!(
                parse_known_hosts_line(line),
                Err(ParseError::MissingField(KnownHostField::Hostnames))
            );
        }
    }

    #[test]
    fn missing_keytype_error() {
        assert_eq!(
            parse_known_hosts_line("example.com"),
            Err(ParseError::MissingField(KnownHostField::Keytype))
        );
    }

    #[test]
    fn missing_key_error() {
        assert_eq!(
            parse_known_hosts_line("example.com ssh-ed25519"),
            Err(ParseError::MissingField(KnownHostField::Key))
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
            vec![
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
            ]
        );
    }

    #[test]
    fn bundled_keys_parse() {
        let parsed = get_bundled_keys();
        assert_eq!(
            parsed.len(),
            BUNDLED_KEYS.len(),
            "some lines failed to parse"
        );
    }

    #[test]
    fn bundled_keys_have_valid_keytypes() {
        let known_keys = [
            SshHostKeyType::Unknown,
            SshHostKeyType::Rsa,
            SshHostKeyType::Dss,
            SshHostKeyType::Ecdsa256,
            SshHostKeyType::Ecdsa384,
            SshHostKeyType::Ecdsa521,
            SshHostKeyType::Ed255219,
        ];
        for key in BUNDLED_KEYS {
            let keytype = key.keytype;
            let is_valid = known_keys.iter().any(|known| known.name() == keytype);
            assert!(is_valid, "{key:?} has invalid keytype");
        }
    }
}
