// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Helper for checking whether a certificate is accepted.

use git2::cert::SshHostKeyType;
use hmac::{Hmac, Mac};
use regex::Regex;
use sha1::Sha1;

use super::{HashedHostname, Hostname, KnownHostEntry, Marker, NormalHostname};

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
/// Result of [`accept`].
///
/// [`accept`]: KnownHostEntry::accept
pub(super) enum AcceptResult {
    /// Host was accepted.
    Accepted,
    /// Host was rejected.
    Rejected,
    /// Host didn't match.
    NotMatched,
    /// The key was revoked.
    Revoked,
    /// There was a key mismatch.
    KeyMismatch,
    /// Unsupported.
    Unsupported,
}

impl KnownHostEntry {
    /// Check whether this [`KnownHostEntry`] accepts the host.
    pub(super) fn accept(
        &self,
        hostname: &str,
        hostkey: &[u8],
        host_type: SshHostKeyType,
    ) -> AcceptResult {
        if self.marker == Some(Marker::Revoked) {
            return AcceptResult::Revoked;
        }
        if self.marker == Some(Marker::CA) {
            return AcceptResult::Unsupported;
        }
        let mut matched = false;

        for host in &self.hosts {
            let result = host.accept(hostname);
            match result {
                AcceptResult::Accepted => matched = true,

                AcceptResult::Rejected
                | AcceptResult::Revoked
                | AcceptResult::KeyMismatch
                | AcceptResult::Unsupported => {
                    return result;
                }

                AcceptResult::NotMatched => {}
            }
        }

        if !matched {
            return AcceptResult::NotMatched;
        }

        if self.key_type != host_type.name() {
            return AcceptResult::KeyMismatch;
        }

        if self.key != hostkey {
            return AcceptResult::KeyMismatch;
        }

        AcceptResult::Accepted
    }
}

impl Hostname {
    fn accept(&self, hostname: &str) -> AcceptResult {
        match self {
            Hostname::Hashed(hashed) => hashed.accept(hostname),
            Hostname::Normal(normal) => normal.accept(hostname),
        }
    }
}

impl NormalHostname {
    fn accept(&self, hostname: &str) -> AcceptResult {
        if !wildcard_matches(&self.pattern, hostname) {
            return AcceptResult::NotMatched;
        }

        if self.is_negated {
            AcceptResult::Rejected
        } else {
            AcceptResult::Accepted
        }
    }
}

impl HashedHostname {
    fn accept(&self, hostname: &str) -> AcceptResult {
        let Ok(mut mac) = Hmac::<Sha1>::new_from_slice(&self.salt) else {
            return AcceptResult::NotMatched;
        };

        mac.update(hostname.as_bytes());

        let result = mac.finalize().into_bytes();

        if result.as_slice() == self.host.as_slice() {
            AcceptResult::Accepted
        } else {
            AcceptResult::NotMatched
        }
    }
}

fn wildcard_matches(pattern: &str, hostname: &str) -> bool {
    let mut regex_pattern = String::from("^");

    for ch in pattern.chars() {
        match ch {
            '*' => regex_pattern.push_str(".*"),
            '?' => regex_pattern.push('.'),
            ch => regex_pattern.push_str(&regex::escape(&ch.to_string())),
        }
    }

    regex_pattern.push('$');

    Regex::new(&regex_pattern)
        .map(|regex| regex.is_match(hostname))
        .unwrap_or(false)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry() -> KnownHostEntry {
        KnownHostEntry {
            marker: None,
            hosts: vec![Hostname::Normal(NormalHostname {
                pattern: "*.example.com".to_string(),
                is_negated: false,
            })],
            key_type: "ssh-ed25519".to_string(),
            key: vec![1, 2, 3, 4],
        }
    }

    #[test]
    fn accepts_matching_hostname_and_key() {
        let entry = entry();

        assert_eq!(
            entry.accept(
                "server.example.com",
                &[1, 2, 3, 4],
                SshHostKeyType::Ed255219
            ),
            AcceptResult::Accepted
        );
    }

    #[test]
    fn rejects_non_matching_hostname() {
        let entry = entry();

        assert_eq!(
            entry.accept("server.other.com", &[1, 2, 3, 4], SshHostKeyType::Ed255219),
            AcceptResult::NotMatched
        );
    }

    #[test]
    fn rejects_different_host_key() {
        let entry = entry();

        assert_eq!(
            entry.accept(
                "server.example.com",
                &[5, 6, 7, 8],
                SshHostKeyType::Ed255219
            ),
            AcceptResult::KeyMismatch
        );
    }

    #[test]
    fn accepts_multiple_matching_hostnames() {
        let entry = KnownHostEntry {
            marker: None,
            hosts: vec![
                Hostname::Normal(NormalHostname {
                    pattern: "*.example.net".to_string(),
                    is_negated: false,
                }),
                Hostname::Normal(NormalHostname {
                    pattern: "server.example.com".to_string(),
                    is_negated: false,
                }),
            ],
            key_type: "ssh-ed25519".to_string(),
            key: vec![1, 2, 3, 4],
        };

        assert_eq!(
            entry.accept(
                "server.example.com",
                &[1, 2, 3, 4],
                SshHostKeyType::Ed255219
            ),
            AcceptResult::Accepted
        );
    }

    #[test]
    fn rejects_negated_entry() {
        let negated_entry = KnownHostEntry {
            marker: None,
            hosts: vec![
                Hostname::Normal(NormalHostname {
                    pattern: "*.example.com".to_string(),
                    is_negated: false,
                }),
                Hostname::Normal(NormalHostname {
                    pattern: "server.example.com".to_string(),
                    is_negated: true,
                }),
            ],
            key_type: "ssh-ed25519".to_string(),
            key: vec![1, 2, 3, 4],
        };

        assert_eq!(
            negated_entry.accept(
                "server.example.com",
                &[1, 2, 3, 4],
                SshHostKeyType::Ed255219
            ),
            AcceptResult::Rejected
        );
    }

    #[test]
    fn rejects_revoked_entry() {
        let revoked_entry = KnownHostEntry {
            marker: Some(Marker::Revoked),
            hosts: vec![Hostname::Normal(NormalHostname {
                pattern: "server.example.com".to_string(),
                is_negated: false,
            })],
            key_type: "ssh-ed25519".to_string(),
            key: vec![1, 2, 3, 4],
        };

        assert_eq!(
            revoked_entry.accept(
                "server.example.com",
                &[1, 2, 3, 4],
                SshHostKeyType::Ed255219
            ),
            AcceptResult::Revoked
        );
    }
}
