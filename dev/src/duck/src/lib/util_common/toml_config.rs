use std::{
    fmt::Display,
    io::ErrorKind,
    path::{Path, PathBuf},
};

use toml::{Table, Value, from_str};
use tracing::debug;

use super::DescriptionWithAnArticle;

use crate::{
    QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    util_common::path_ops_ext::PathOpsExt,
};
use paste::item;
use toml::value::{Array, Datetime};

#[derive(Default, Debug)]
pub struct TomlConfig {
    content: Table,
    source: Option<PathBuf>,
}

impl DescriptionWithAnArticle for Value {
    fn desc_with_article(&self) -> &'static str {
        match self {
            Value::String(..) => "a string",
            Value::Integer(..) => "an integer",
            Value::Float(..) => "a float",
            Value::Boolean(..) => "a boolean",
            Value::Datetime(..) => "a datetime",
            Value::Array(..) => "an array",
            Value::Table(..) => "a table",
        }
    }
}

macro_rules! delegate_getter {
    (
        $(
            $name:ident => $toml_value_fn:ident -> $ret:ty: $human_type:literal $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<get_ $name>](&self, key: &str) -> QuackResult<Option<$ret>> {
                    let value = self.get(key)?;
                    let Some(value) = value else {
                        return Ok(None);
                    };
                    match value.[<as_ $toml_value_fn>]() {
                        Some(x) => Ok(Some(x)),
                        None => Err(qp_err!("{}", self.make_location_error())).context(
                            format!("the key `{key}` expects {}, not {}", $human_type, value.desc_with_article())
                        )
                    }
                }
            )*
        }
    };
}

macro_rules! delegate_setter {
    (
        $(
            $name:ident => $toml_value_enum:ident -> $value:ty $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<set_ $name>](&mut self, key: &str, value: $value) -> QuackResult<()> {
                    let value = Value::$toml_value_enum(value);
                    self.set(key, value)
                }
            )*
        }
    };
}

impl TomlConfig {
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        debug!("parsing TOML config at `{}`", path.display());
        let content = match path.as_path().read_to_string() {
            Ok(string) => string,
            Err(e) if matches!(e.source().kind(), ErrorKind::NotFound) => {
                debug!(
                    "there is no config at `{}`, falling back to defaults...",
                    path.display()
                );
                return Ok(Self::default());
            }
            Err(e) => {
                return Err(e).context(format!(
                    "when trying to read a user config at `{}`",
                    path.display()
                ));
            }
        };
        let content = from_str::<Table>(&content).with_context(|| {
            format!(
                "when trying to parse a user config at `{}` into the TOML table",
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
            Some(ref path) => format!("when parsing the configuration at `{}`", path.display()),
            None => {
                "You've encountered an internal error: when parsing the default user configuration"
                    .into()
            }
        }
    }

    #[track_caller]
    fn _get(&self, key: &str) -> QuackResult<Option<&Value>> {
        debug!(
            "getting the key `{key}` from config at `{}`",
            self.source
                .as_ref()
                .map(|buf| buf.display())
                .unwrap_or_else(|| Path::new("<default-config>").display()) // It's dyn-hack.
        );
        if key.is_empty() {
            qp_bail_internal!("empty key")
        }
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so split should return at least one element"
            )
        };
        let mut current: &Table = &self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, key));
            }
            let Some(next) = current.get(part) else {
                debug!(
                    "there is no table `[{part}]` in the chain `{}`",
                    parts[0..=i].join("."),
                );
                return Ok(None);
            };
            let Value::Table(next) = next else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {}",
                    parts[0..=i].join("."),
                    next.desc_with_article(),
                )
            };
            current = next;
        }
        Ok(current.get(last))
    }

    fn get(&self, key: &str) -> QuackResult<Option<&Value>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    #[track_caller]
    fn _set(&mut self, key: &str, value: Value) -> QuackResult<()> {
        if key.is_empty() {
            qp_bail_internal!("empty key")
        }
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so split should return at least one element"
            )
        };
        let mut current = &mut self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, key));
            }
            let next = current
                .entry(part)
                .or_insert_with(|| Value::Table(Default::default()));
            let next_type = next.desc_with_article();
            let Some(next) = next.as_table_mut() else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {}",
                    parts[0..=i].join("."),
                    next_type,
                )
            };
            current = next;
        }
        let _ = current.insert(last.to_string(), value);
        Ok(())
    }

    fn set(&mut self, key: &str, value: Value) -> QuackResult<()> {
        self._set(key, value)
            .with_context(|| self.make_location_error())
    }

    fn make_empty_key_fragment_error(mut i: usize, key: &str) -> QuackError {
        i += 1;
        let last_two = i % 100;
        let digit = last_two % 10;
        let decimal = last_two - digit;
        let suffix = match digit {
            1 if decimal != 10 => "st",
            2 if decimal != 10 => "nd",
            3 if decimal != 10 => "rd",
            _ => "th",
        };
        qp_err!("{i}{suffix} part of the key `{key}` is empty")
    }

    delegate_getter! {
        str => str -> &str: "a string",
        array => array -> &Array: "an array",
        table => table -> &Table: "a table",
        date => datetime -> &Datetime: "a datetime",
        int => integer -> i64: "an integer",
        float => float -> f64: "a float",
        bool => bool -> bool: "a boolean",
    }

    delegate_setter! {
        str => String -> String,
        array => Array -> Array,
        table => Table -> Table,
        date => Datetime -> Datetime,
        int => Integer -> i64,
        float => Float -> f64,
        bool => Boolean -> bool,
    }

    pub fn get_root_table(&self) -> &Table {
        &self.content
    }

    pub fn get_path(&self, key: &str) -> QuackResult<Option<&Path>> {
        let path = self.get_str(key)?;
        Ok(path.map(Path::new))
    }

    pub fn set_path(&mut self, key: &str, value: &Path) -> QuackResult<()> {
        self.set_str(key, value.display().to_string())
    }
}

impl Display for TomlConfig {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.content.fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;

    fn prepare_file(content: &str) -> NamedTempFile {
        use std::io::Write;
        let mut file = NamedTempFile::new().expect("couldn't create a tempfile");
        file.write_all(content.as_bytes())
            .expect("couldn't write to the tempfile");
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
        assert_eq!(
            config.get_int("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_table("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_array("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_bool("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_float("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_date("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_str("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a string",
                file.path().display()
            )
        );
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

    #[test]
    fn basic_set() {
        let file = prepare_file(
            r#"
        a = 1
        b = 1.2
        c = "xd"
        d = "dx"
        [foo]
        bar = "xd"
        xd = "c"
        [foo.a]
        a = 1
        "#,
        );
        let mut config = TomlConfig::new(file.as_ref().to_path_buf()).unwrap();
        config.set_bool("a", true).unwrap();
        config.set_int("b", 2137).unwrap();
        config.set_table("c", Table::new()).unwrap();
        config.set_array("foo", Array::new()).unwrap();
        config
            .set_table(
                "e.bar.xd",
                Table::from_iter([(String::from("a"), Value::Integer(1))]),
            )
            .unwrap();
        let err = config.set_int("foo.a", 1).unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo` expected a table, not an array",
                file.path().display()
            )
        );
        assert_eq!(
            config.to_string(),
            r#"a = true
b = 2137
d = "dx"
foo = []

[c]

[e.bar.xd]
a = 1
"#
        );
    }
}
