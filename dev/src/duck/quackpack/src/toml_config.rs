use std::{
    io::ErrorKind,
    path::{Path, PathBuf},
};

use anyhow::{Context, anyhow, bail};
use rustvil::fs::PathExt;
use toml::{Table, Value, from_str};
use tracing::debug;

use crate::QuackResult;
use paste::item;
use toml::value::{Array, Datetime};

#[derive(Default, Debug)]
pub struct TomlConfig {
    content: Table,
    source: Option<PathBuf>,
}

macro_rules! delegate_getter {
    (
        $(
            $name:ident => $toml_value_fn:ident -> $ret:ty $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<get_ $name>](&self, key: &str) -> QuackResult<Option<$ret>> {
                    let value = self.get(key)?;
                    let Some(value) = value else { return Ok(None); };
                    match value.[<as_ $toml_value_fn>]() {
                        Some(x) => Ok(Some(x)),
                        // TODO: Right now $toml_value_fn is human readable; maybe add another parameter for displaying?
                        None => Err(anyhow!(self.make_location_error()))
                                    .context(
                                        format!("when getting key `{key}` expected {}, not a {}", stringify!($toml_value_fn), value.type_str())
                                    )
                    }
                }
            )*
        }
    };
}

#[cfg(feature = "test_utils")]
#[doc(hidden)]
macro_rules! delegate_setter {
    (
        $(
            $name:ident, $value_type:ty $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<set_ $name>](&mut self, key: String, value: $value_type) {
                    let wrapped_value = Value::try_from(value).unwrap();
                    let _ = self.set(key.clone(), wrapped_value);
                }
            )*
        }
    };
}

impl TomlConfig {
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        let content = match path.as_path().read_to_string() {
            Ok(string) => string,
            Err(e) if matches!(e.kind(), ErrorKind::NotFound) => {
                debug!(
                    "there is no config at `{}`, falling back to defaults...",
                    path.display()
                );
                return Ok(Self::default());
            }
            Err(e) => {
                return Err(e).context(format!(
                    "when trying to read user config at `{}`",
                    path.display()
                ));
            }
        };
        let content = from_str::<Table>(&content).with_context(|| {
            format!(
                "when trying to parse user config at `{}` into a TOML table",
                path.display()
            )
        })?;
        Ok(Self {
            content,
            source: Some(path),
        })
    }

    pub fn make_location_error(&self) -> String {
        match self.source {
            Some(ref path) => format!("when parsing configuration at `{}`", path.display()),
            None => {
                "You've encountered internal error: when parsing default user configuration".into()
            }
        }
    }

    #[track_caller]
    fn _get<'a>(&'a self, key: &str) -> QuackResult<Option<&'a Value>> {
        debug!(
            "getting key `{key}` from config at `{}`",
            self.source
                .as_ref()
                .map(|buf| buf.display())
                .unwrap_or_else(|| Path::new("<default-config>").display()) // It's dyn-hack.
        );
        if key.is_empty() {
            bail!("empty key")
        }
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we asserted that key is not empty, so split should return at least one element"
            )
        };
        let mut current: &Table = &self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                Self::error_empty_key_fragment(i, key)?
            }
            let Some(next) = current.get(part) else {
                debug!(
                    "there is no table `[{part}]` in chain `{}`",
                    parts[0..=i].join(".")
                );
                return Ok(None);
            };
            let Value::Table(next) = next else {
                bail!(
                    "in chain `{}` expected table, not a {}",
                    parts[0..=i].join("."),
                    next.type_str()
                )
            };
            current = next;
        }
        Ok(current.get(last))
    }

    fn get<'a>(&'a self, key: &str) -> QuackResult<Option<&'a Value>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    #[cfg(feature = "test_utils")]
    #[doc(hidden)]
    fn _set(&mut self, key: String, value: Value) -> QuackResult<()> {
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we asserted that key is not empty, so split should return at least one element"
            )
        };
        let mut current = &mut self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                Self::error_empty_key_fragment(i, &key)?
            }
            if !current.contains_key(part) {
                current.insert(part.to_string(), Value::Table(Table::new()));
            }
            let Some(next) = current.get_mut(part) else {
                unreachable!("We've just inserted part into current");
            };
            let next_type = next.type_str();
            let Some(next) = next.as_table_mut() else {
                bail!(
                    "in chain `{}` expected table, not a {}",
                    parts[0..=i].join("."),
                    next_type
                )
            };
            current = next;
        }
        let _ = current.insert(last.to_string(), value);
        Ok(())
    }

    #[cfg(feature = "test_utils")]
    #[doc(hidden)]
    fn set(&mut self, key: String, value: Value) {
        let _ = self._set(key, value);
    }

    fn error_empty_key_fragment(i: usize, key: &str) -> QuackResult<()> {
        let i = i + 1;
        let last_two = i % 100;
        let digit = last_two % 10;
        let decimal = last_two - digit;
        let suffix = match digit {
            1 if decimal != 10 => "st",
            2 if decimal != 10 => "nd",
            3 if decimal != 10 => "rd",
            _ => "th",
        };
        bail!("{i}{suffix} part of key `{key}` is empty")
    }

    delegate_getter! {
        str => str -> &str,
        array => array -> &Array,
        table => table -> &Table,
        date => datetime -> &Datetime,
        int => integer -> i64,
        float => float -> f64,
        bool => bool -> bool,
    }

    #[cfg(feature = "test_utils")]
    delegate_setter! {
        str, &str,
        array, &Array,
        table, &Table,
        date, &Datetime,
        int, i64,
        float, f64,
        bool, bool,
    }

    pub fn get_root_table(&self) -> &Table {
        &self.content
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;

    fn prepare_file(content: &str) -> NamedTempFile {
        use std::io::Write;
        let mut file = NamedTempFile::new().expect("couldn't create tempfile");
        file.write_all(content.as_bytes())
            .expect("couldn't write to file");
        file
    }

    #[test]
    fn test_empty() {
        let file = prepare_file("");
        let config = TomlConfig::new(file.as_ref().to_path_buf()).unwrap();
        assert!(matches!(config.get_table("a"), Ok(None)));
        assert!(matches!(config.get_array("a"), Ok(None)));
        assert!(matches!(config.get_str("a"), Ok(None)));
        assert!(matches!(config.get_float("a"), Ok(None)));
        assert!(matches!(config.get_bool("a"), Ok(None)));
        assert!(matches!(config.get_date("a"), Ok(None)));
    }

    #[test]
    fn test_basic() {
        let file = prepare_file(
            r#"
        a = 1
        b = 1.2
        c = "xd"
        [foo]
        bar = "xd"
        xd = "c"
        [foo.a]
        a = 1
        "#,
        );
        let config = TomlConfig::new(file.as_ref().to_path_buf()).unwrap();
        assert!(matches!(config.get_table("foo"), Ok(Some(_))));
        assert!(matches!(config.get_table("foo.a"), Ok(Some(_))));
        assert!(matches!(config.get_str("c"), Ok(Some("xd"))));
        assert!(matches!(config.get_float("b"), Ok(Some(1.2))));
        assert!(matches!(config.get_int("a"), Ok(Some(1))));
        assert!(matches!(config.get_int("foo.a.a"), Ok(Some(1))));
        assert!(config.get_int("foo.xd.a").is_err());
        assert!(config.get_table("foo.xd.a").is_err());
        assert!(config.get_array("foo.xd.a").is_err());
        assert!(config.get_bool("foo.xd.a").is_err());
        assert!(config.get_float("foo.xd.a").is_err());
        assert!(config.get_date("foo.xd.a").is_err());
        assert!(config.get_str("foo.xd.a").is_err());
        assert_eq!(
            config
                .get_table("foo")
                .unwrap()
                .unwrap()
                .get("xd")
                .unwrap()
                .as_str(),
            Some("c")
        );
    }
}
