// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Parse a known-hosts file.
//! All sections are copied from: <https://man.openbsd.org/sshd#SSH_KNOWN_HOSTS_FILE_FORMAT>.

#![expect(dead_code)]

use tracing::{debug, trace};

// The marker is optional, but if it is present then it must be one of “@cert-authority”,
// to indicate that the line contains a certification authority (CA) key, or “@revoked”,
// to indicate that the key contained on the line is revoked and must not ever be accepted.
const CA_MARKER: &str = "@cert-authority";
const REVOKED_MARKER: &str = "@revoked";

// https://github.com/openssh/openssh-portable/blob/1fbdcb402516aacce59b4cc7b7d98f06820a1e44/hostfile.h#L59-L60
const HASH_MAGIC: &str = "|1|";
const HASH_DELIMITER: char = '|';

// Hostnames is a comma-separated list of patterns...
const HOSTNAMES_DELIMITER: char = ',';

// A pattern may also be preceded by ‘!’ to indicate negation...
const NEGATED_HOSTNAME: char = '!';

type ParseResult<T> = Result<T, ParseError>;

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub enum ParseError {
    MissingField(MissingFieldType),
    MissingHashMagic,
    MissingHashSaltOrHost,
    TooManyHashDelims,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub enum MissingFieldType {
    Hostnames,
    Keytype,
    Key,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub enum KnownHostsLine {
    Comment,
    Entry(KnownHostEntry),
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub enum Marker {
    CA,
    Revoked,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct NormalHostname {
    name: String,
    is_negated: bool,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct HashedHostname {
    salt: String,
    host: String,
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub enum Hostname {
    Hashed(HashedHostname),
    Normal(NormalHostname),
}

#[derive(Debug)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct KnownHostEntry {
    marker: Option<Marker>,
    hosts: Vec<Hostname>,
    key_type: String,
    key: String,
}

pub fn parse_known_hosts_file(file: &str) -> ParseResult<Vec<KnownHostsLine>> {
    file.lines().map(parse_known_hosts_line).collect()
}

pub fn parse_known_hosts_line(line: &str) -> ParseResult<KnownHostsLine> {
    let line = line.trim_ascii();
    trace!(target: "duck::git", ?line, "parsing line");
    // Lines starting with ‘#’ and empty lines are ignored as comments.
    if line.is_empty() || line.starts_with('#') {
        return Ok(KnownHostsLine::Comment);
    }
    // Each line in these files contains the following fields: marker (optional), hostnames, keytype,
    // base64-encoded key, comment. The fields are separated by spaces.
    let mut iter = line.split_ascii_whitespace();
    let marker_or_hostnames = iter
        .next()
        .expect("line was trimmed and is nonempty, should yield at least one element");

    // The marker is optional, but if it is present then it must be one of “@cert-authority”,
    // to indicate that the line contains a certification authority (CA) key, or “@revoked”,
    // to indicate that the key contained on the line is revoked and must not ever be accepted.
    // Only one marker should be used on a key line.
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
    // @TODO: #3153 Decode.
    let key = base64_key;
    let entry = KnownHostEntry {
        marker,
        hosts,
        key_type: key_type.to_string(),
        key: key.to_string(),
    };
    trace!(target: "duck::git", ?entry, "parsed known-hosts entry");
    Ok(KnownHostsLine::Entry(entry))
}

fn parse_hostnames(hostnames: &str) -> ParseResult<Vec<Hostname>> {
    // Alternately, hostnames may be stored in a hashed form which hides host names and addresses
    // should the file's contents be disclosed. Hashed hostnames start with a ‘|’ character.
    // Only one hashed hostname may appear on a single line and none of the above negation
    // or wildcard operators may be applied.
    if hostnames.starts_with(HASH_DELIMITER) {
        debug!(target: "duck::git", "detected hashed hostname");
        let hashed = parse_hashed_hostname(hostnames)?;
        trace!(target: "duck::git", ?hashed, "parsed hashed hostname");
        return Ok(vec![Hostname::Hashed(hashed)]);
    }
    let mut parsed = vec![];
    // Hostnames is a comma-separated list of patterns...
    for hostname in hostnames.split(HOSTNAMES_DELIMITER) {
        // A pattern may also be preceded by ‘!’ to indicate negation...
        let (is_negated, hostname) = match hostname.strip_prefix(NEGATED_HOSTNAME) {
            Some(stripped) => (true, stripped),
            None => (false, hostname),
        };
        let host = NormalHostname {
            name: hostname.to_string(),
            is_negated,
        };
        trace!(target: "duck::git", ?host, "parsed normal host");
        parsed.push(Hostname::Normal(host));
    }
    Ok(parsed)
}

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
    Ok(HashedHostname {
        // @TODO: #3153 These are base64 encoded.
        salt: salt.to_string(),
        host: host.to_string(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    const KEY_TYPE: &str = "ssh-ed25519";
    // Duckling in base64.
    const KEY: &str = "RHVja2xpbmc=";

    fn normal(name: &str, is_negated: bool) -> Hostname {
        Hostname::Normal(NormalHostname {
            name: name.to_string(),
            is_negated,
        })
    }

    fn hashed(salt: &str, host: &str) -> Hostname {
        Hostname::Hashed(HashedHostname {
            salt: salt.to_string(),
            host: host.to_string(),
        })
    }

    fn entry(
        marker: Option<Marker>,
        hosts: Vec<Hostname>,
        key_type: &str,
        key: &str,
    ) -> KnownHostsLine {
        KnownHostsLine::Entry(KnownHostEntry {
            marker,
            hosts,
            key_type: key_type.to_string(),
            key: key.to_string(),
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
                KEY,
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
                KEY,
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
                KEY,
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
                KEY,
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
                KEY,
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
                KEY,
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
                KEY,
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
                KEY,
            ))
        );
    }

    #[test]
    fn hashed_hostname() {
        assert_eq!(
            parse_known_hosts_line(&format!("|1|some-salt|some-hashed-host {KEY_TYPE} {KEY}")),
            Ok(entry(
                None,
                vec![hashed("some-salt", "some-hashed-host")],
                KEY_TYPE,
                KEY,
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
                entry(None, vec![normal("example.com", false)], KEY_TYPE, KEY,),
                entry(
                    Some(Marker::CA),
                    vec![normal("*.example.com", false)],
                    KEY_TYPE,
                    KEY,
                ),
                entry(
                    Some(Marker::Revoked),
                    vec![normal("revoked.example.com", false)],
                    KEY_TYPE,
                    KEY,
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
        let salt = "JfKTdBh7rNbXkVAQCRp4OQoPfmI=";
        let host = "USECr3SWf1JUPsms5AqfD5QfxkM=";
        let file = format!(
            "\
# Comments allowed at start of line
cvs.example.net,192.0.2.10 {KEY_TYPE} {KEY}
# A hashed hostname
|1|{salt}|{host} {KEY_TYPE} {KEY}
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
                    KEY,
                ),
                KnownHostsLine::Comment,
                entry(None, vec![hashed(salt, host)], KEY_TYPE, KEY,),
                KnownHostsLine::Comment,
                entry(
                    Some(Marker::Revoked),
                    vec![normal("*", false)],
                    KEY_TYPE,
                    KEY,
                ),
                KnownHostsLine::Comment,
                entry(
                    Some(Marker::CA),
                    vec![
                        normal("*.mydomain.org", false),
                        normal("*.mydomain.com", false)
                    ],
                    KEY_TYPE,
                    KEY,
                ),
            ])
        );
    }
}
