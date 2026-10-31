// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Cross-platform environment variables snapshot.
use std::collections::HashMap;
use std::ffi::{OsStr, OsString};
use std::fmt;
use std::str::FromStr;
use std::time::Duration;

use url::Url;

use crate::quackpack::schemas::config::HumanDeserializableDuration;
use crate::{QuackResult, QuackResultContext, qp_err};

/// Safe wrapper around [`std::env::vars_os`], which is safe to access on Windows: some of its
/// environmental variables are case-insensitive.
#[derive(Clone, PartialEq, Eq)]
pub struct Env {
    env: HashMap<OsString, OsString>,

    // Map from normalised keys (uppercase) to original.
    normalised_keys: HashMap<String, String>,
}

impl Env {
    /// Create a new default [`Env`].
    pub fn new() -> Self {
        Self::new_from(std::env::vars_os().collect())
    }

    /// Create new [`Env`] using `env` as existing environmental variables.
    pub fn new_from(env: HashMap<OsString, OsString>) -> Self {
        let normalised_keys = Env::normalize_keys(&env);
        Self {
            env,
            normalised_keys,
        }
    }

    fn normalize_keys(keys: &HashMap<OsString, OsString>) -> HashMap<String, String> {
        keys.keys()
            .filter_map(|k| k.to_str())
            .map(|k| (k.to_uppercase(), k.to_owned()))
            .collect()
    }

    /// Reload environmental variables from `env`.
    pub fn reload_from(&mut self, env: HashMap<OsString, OsString>) {
        let normalised = Env::normalize_keys(&env);
        self.env = env;
        self.normalised_keys = normalised;
    }

    /// Reload environmental variables from [`std::env::vars_os`].
    pub fn reload(&mut self) {
        self.reload_from(std::env::vars_os().collect())
    }

    /// Get environmental variable pointed by `key`.
    ///
    /// ## Arguments
    ///
    /// * `key` - key for environmental variable. Must implement [`AsRef<OsStr>`].
    ///
    /// ## Returns
    /// [`Option<&OsStr>`]. [`None`] variant indicates missing key, [`Some`]: existing key.
    ///
    pub fn get_os(&self, key: impl AsRef<OsStr>) -> Option<&OsStr> {
        let key = key.as_ref();
        match self.env.get(key) {
            Some(x) => Some(x),
            None => {
                if cfg!(windows) {
                    self.get_normalised(key)
                } else {
                    None
                }
            }
        }
    }

    fn get_normalised(&self, key: &OsStr) -> Option<&OsStr> {
        let k = key.to_str()?.to_uppercase();
        let env_key: &OsStr = self.normalised_keys.get(&k)?.as_ref();
        self.env.get(env_key).map(OsString::as_ref)
    }

    /// Check, whether this [`Env`] has key `key`.
    pub fn has(&self, key: impl AsRef<OsStr>) -> bool {
        self.get_os(key).is_some()
    }

    /// Get environmental variable pointed by `key` and convert it to utf8.
    /// Returns [`None`] if no variable is found.
    ///
    /// ## Arguments
    ///
    /// * `key` - key for environmental variable.
    fn get(&self, key: &OsStr) -> QuackResult<Option<&str>> {
        let display = key.display();
        self.get_os(key)
            .map(|res| {
                res.to_str().ok_or_else(|| {
                    qp_err!("environmental variable `{display}` is not a utf8 string")
                })
            })
            .transpose()
    }

    /// Get the [`str`] from an environmental variable.
    /// Returns [`None`] if no variable is found.
    pub fn get_str(&self, key: impl AsRef<OsStr>) -> QuackResult<Option<&str>> {
        self.get(key.as_ref()).with_context(|| {
            format!(
                "when reading the environmental variable {}",
                key.as_ref().display()
            )
        })
    }

    /// Get the [`i64`] from an environmental variable.
    /// Returns [`None`] if no variable is found.
    pub fn get_i64(&self, key: impl AsRef<OsStr>) -> QuackResult<Option<i64>> {
        self.get(key.as_ref())?
            .map(str::parse::<i64>)
            .transpose()
            .with_context(|| {
                format!(
                    "when reading the environmental variable {}",
                    key.as_ref().display()
                )
            })
    }

    /// Get the [`bool`] from an environmental variable.
    /// Returns [`None`] if no variable is found.
    pub fn get_bool(&self, key: impl AsRef<OsStr>) -> QuackResult<Option<bool>> {
        self.get(key.as_ref())?
            .map(str::parse::<bool>)
            .transpose()
            .with_context(|| {
                format!(
                    "when reading the environmental variable {}",
                    key.as_ref().display()
                )
            })
    }

    /// Get the [`Duration`] from an environmental variable.
    /// Returns [`None`] if no variable is found.
    pub fn get_duration(&self, key: impl AsRef<OsStr>) -> QuackResult<Option<Duration>> {
        let Some(duration_str) = self.get(key.as_ref())? else {
            return Ok(None);
        };
        let duration = HumanDeserializableDuration::from_str(duration_str).with_context(|| {
            format!(
                "when reading the environmental variable {}",
                key.as_ref().display()
            )
        })?;
        Ok(Some(duration.0))
    }

    /// Get the [`Url`] from an environmental variable.
    /// Returns [`None`] if no variable is found.
    pub fn get_url(&self, key: impl AsRef<OsStr>) -> QuackResult<Option<Url>> {
        let Some(url_str) = self.get(key.as_ref())? else {
            return Ok(None);
        };
        let url = Url::parse(url_str).with_context(|| {
            format!(
                "when reading the environmental variable {}",
                key.as_ref().display()
            )
        })?;
        Ok(Some(url))
    }

    fn from_iter<I: Iterator<Item = (OsString, OsString)>>(t: I) -> Self {
        let mut env = HashMap::new();
        let mut normalised_keys = HashMap::new();
        for (key, value) in t {
            if let Some(key) = key.to_str() {
                normalised_keys.insert(key.to_uppercase(), key.to_owned());
            }
            env.insert(key, value);
        }
        Self {
            env,
            normalised_keys,
        }
    }
}

impl Default for Env {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
impl Env {
    pub fn empty() -> Self {
        Self {
            env: HashMap::new(),
            normalised_keys: HashMap::new(),
        }
    }
}

impl FromIterator<(OsString, OsString)> for Env {
    fn from_iter<T: IntoIterator<Item = (OsString, OsString)>>(iter: T) -> Self {
        Self::from_iter(iter.into_iter())
    }
}

impl<const N: usize> From<[(OsString, OsString); N]> for Env {
    fn from(value: [(OsString, OsString); N]) -> Self {
        <Self as FromIterator<(OsString, OsString)>>::from_iter(value)
    }
}

impl fmt::Debug for Env {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Env").finish_non_exhaustive()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn make_dummy_env() -> Env {
        Env::from([(OsString::from("ala"), OsString::from("bar"))])
    }

    #[test]
    fn basic_test() {
        let env = make_dummy_env();
        assert!(env.has("ala"));
        assert_eq!(env.get_os("ala"), Some(OsStr::new("bar")));
        assert!(matches!(env.get_str("ala"), Ok(Some("bar"))));
        if cfg!(windows) {
            assert!(env.has("aLA"));
            assert_eq!(env.get_os("aLA"), Some(OsStr::new("bar")));
            assert!(matches!(env.get_str("aLA"), Ok(Some("bar"))));
        } else {
            assert!(!env.has("aLA"));
            assert_eq!(env.get_os("aLA"), None);
        }
    }
}
