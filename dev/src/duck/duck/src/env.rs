use std::collections::HashMap;
use std::ffi::{OsStr, OsString};

use thiserror::Error;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Env {
    env: HashMap<OsString, OsString>,

    normalised_keys: HashMap<String, String>,
}

#[derive(Debug, Clone, Error, PartialEq, Eq, Hash)]
pub enum EnvStrError {
    #[error("there is no environmental variable `${0:?}`")]
    Missing(OsString),

    #[error("environmental variable `${0:?}` is not an UTF-8 string")]
    NonUTF8(OsString),
}

impl Env {
    pub fn new() -> Self {
        Self::new_from(std::env::vars_os().collect())
    }

    pub fn new_from(env: HashMap<OsString, OsString>) -> Self {
        let normalised = Env::normalize_map(&env);
        Self {
            env,
            normalised_keys: normalised,
        }
    }

    fn normalize_map(keys: &HashMap<OsString, OsString>) -> HashMap<String, String> {
        keys.keys()
            .filter_map(|k| k.to_str())
            .map(|k| (k.to_uppercase(), k.to_owned()))
            .collect()
    }

    pub fn reload_from(&mut self, env: HashMap<OsString, OsString>) {
        let normalised = Env::normalize_map(&env);
        self.env = env;
        self.normalised_keys = normalised;
    }

    pub fn reload(&mut self) {
        self.reload_from(std::env::vars_os().collect())
    }

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
        self.env.get(env_key).map(|v| v.as_ref())
    }

    pub fn has(&self, key: impl AsRef<OsStr>) -> bool {
        self.get_os(key).is_some()
    }

    pub fn get(&self, key: impl AsRef<OsStr>) -> Result<&str, EnvStrError> {
        let key = key.as_ref();
        self.get_os(key)
            .ok_or_else(|| EnvStrError::Missing(key.to_os_string()))?
            .to_str()
            .ok_or_else(|| EnvStrError::NonUTF8(key.to_os_string()))
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
        assert_eq!(env.get("ala"), Ok("bar"));
        if cfg!(windows) {
            assert!(env.has("aLA"));
            assert_eq!(env.get_os("aLA"), Some(OsStr::new("bar")));
            assert_eq!(env.get("aLA"), Ok("bar"));
        } else {
            assert!(!env.has("aLA"));
            assert_eq!(env.get_os("aLA"), None);
            assert_eq!(
                env.get("aLA"),
                Err(EnvStrError::Missing(OsString::from("aLA")))
            );
        }
    }
}
