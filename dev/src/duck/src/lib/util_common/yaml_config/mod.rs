//! Implementation of traversing YAML documents, and getting/setting values at dotted keys.
use std::{
    fmt,
    io::ErrorKind,
    path::{Path, PathBuf},
};
use tracing::debug;

use serde_yaml_ng::{Mapping, Sequence, Value, from_str, to_string};

use crate::{
    QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    util_common::path_ops_ext::PathOpsExt,
};

use super::DescriptionWithAnArticle;

impl DescriptionWithAnArticle for Value {
    fn desc_with_article(&self) -> &'static str {
        match self {
            Self::Null => "a null",
            Self::Bool(..) => "a boolean",
            Self::Number(n) => {
                if n.is_f64() {
                    "a float"
                } else if n.is_u64() {
                    "a positive integer"
                } else {
                    "an integer"
                }
            }
            Self::String(..) => "a string",
            Self::Sequence(..) => "an array",
            Self::Mapping(..) => "a table",
            Self::Tagged(..) => "a tagged value",
        }
    }
}

macro_rules! delegate_getter {
    (
        FunctionName: $name:ident,
        ReturnType: $ret:ty,
        DocType: $doc:ty,
        HumanType: $human_type:literal,
        CastFunctionName: $yaml_value_fn:ident $(,)?
    ) => {
        #[doc = concat!("Get [`", stringify!($doc), "`] at the dotted key.")]
        pub fn $name(&self, key: &str) -> QuackResult<Option<$ret>> {
            let value = self.get(key)?;
            let Some(value) = value else {
                return Ok(None);
            };
            match value.$yaml_value_fn() {
                Some(x) => Ok(Some(x)),
                None => Err(qp_err!("{}", self.make_location_error())).context(format!(
                    "the key `{key}` expects {}, not {}",
                    $human_type,
                    value.desc_with_article()
                )),
            }
        }
    };
}

macro_rules! delegate_setter {
    (
        FunctionName: $name:ident,
        InputType: $value:ty $(,)?
    ) => {
        #[doc = concat!("Set [`", stringify!($value), "`] at the dotted key.")]
        pub fn $name(&mut self, key: &str, value: $value) -> QuackResult<()> {
            let value: Value = value.into();
            self.set(key, value)
        }
    };
}

#[derive(Default, Debug)]
/// YAML config manager.
pub struct YamlConfig {
    content: Mapping,
    source: Option<PathBuf>,
}

impl YamlConfig {
    /// Create a new [`YamlConfig`] from the YAML file at `path`.
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        debug!("parsing YAML config at `{}`", path.display());
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
        let content: Value = from_str(&content).with_context(|| {
            format!(
                "when trying to parse a user config at `{}` into the YAML value",
                path.display()
            )
        })?;
        let content = match content {
            Value::Mapping(mapping) => mapping,
            // Make empty files work.
            Value::Null => return Ok(Self::default()),
            _ => qp_bail!("user config at `{}` is not a YAML table", path.display()),
        };
        Ok(Self {
            content,
            source: Some(path),
        })
    }

    /// Create an error message.
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
    /// Get the value from the dotted key.
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
        let mut current: &Mapping = &self.content;
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
            let Value::Mapping(next) = next else {
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

    /// Convenient wrapper around [`_get`](Self::_get).
    fn get(&self, key: &str) -> QuackResult<Option<&Value>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    #[track_caller]
    /// Set the value at the dotted key.
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
                .entry(part.into())
                .or_insert_with(|| Value::Mapping(Default::default()));

            let next_type = next.desc_with_article();
            let Some(next) = next.as_mapping_mut() else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {}",
                    parts[0..=i].join("."),
                    next_type,
                )
            };
            current = next;
        }
        let _ = current.insert(last.into(), value);
        Ok(())
    }

    /// Convenient wrapper around [`_set`](Self::_set).
    fn set(&mut self, key: &str, value: Value) -> QuackResult<()> {
        self._set(key, value)
            .with_context(|| self.make_location_error())
    }

    /// Make an error message, if i-th part of the key is empty (there are two consecutive dots).
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
        FunctionName: get_str,
        ReturnType: &str,
        DocType: str,
        HumanType: "a string",
        CastFunctionName: as_str,
    }

    delegate_getter! {
        FunctionName: get_array,
        ReturnType: &Sequence,
        DocType: Sequence,
        HumanType: "an array",
        CastFunctionName: as_sequence,
    }

    delegate_getter! {
        FunctionName: get_table,
        ReturnType: &Mapping,
        DocType: Mapping,
        HumanType: "a table",
        CastFunctionName: as_mapping,
    }

    delegate_getter! {
        FunctionName: get_i64,
        ReturnType: i64,
        DocType: i64,
        HumanType: "an integer",
        CastFunctionName: as_i64,
    }

    delegate_getter! {
        FunctionName: get_u64,
        ReturnType: u64,
        DocType: u64,
        HumanType: "a positive integer",
        CastFunctionName: as_u64,
    }

    delegate_getter! {
        FunctionName: get_bool,
        ReturnType: bool,
        DocType: bool,
        HumanType: "a boolean",
        CastFunctionName: as_bool,
    }

    delegate_getter! {
        FunctionName: get_f64,
        ReturnType: f64,
        DocType: f64,
        HumanType: "a float",
        CastFunctionName: as_f64,
    }

    delegate_setter! {
        FunctionName: set_array,
        InputType: Sequence,
    }

    delegate_setter! {
        FunctionName: set_table,
        InputType: Mapping,
    }

    delegate_setter! {
        FunctionName: set_bool,
        InputType: bool,
    }

    delegate_setter! {
        FunctionName: set_i64,
        InputType: i64,
    }

    delegate_setter! {
        FunctionName: set_u64,
        InputType: u64,
    }

    delegate_setter! {
        FunctionName: set_str,
        InputType: String,
    }

    delegate_setter! {
        FunctionName: set_f64,
        InputType: f64,
    }

    /// Get the root [`Mapping`] for this config.
    pub fn get_root_table(&self) -> &Mapping {
        &self.content
    }

    /// Get [`Path`] for the dotted key.
    pub fn get_path(&self, key: &str) -> QuackResult<Option<&Path>> {
        let path = self.get_str(key)?;
        Ok(path.map(Path::new))
    }

    /// Set [`Path`] at the dotted key.
    pub fn set_path(&mut self, key: &str, value: &Path) -> QuackResult<()> {
        self.set_str(key, value.display().to_string())
    }
}

impl fmt::Display for YamlConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let content = to_string(&self.content).map_err(|_| fmt::Error)?;
        write!(f, "{}", content)
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
        let config = YamlConfig::new(file.as_ref().to_path_buf()).unwrap();
        assert!(matches!(config.get_table("a"), Ok(None)));
        assert!(matches!(config.get_array("a"), Ok(None)));
        assert!(matches!(config.get_str("a"), Ok(None)));
        assert!(matches!(config.get_f64("a"), Ok(None)));
        assert!(matches!(config.get_bool("a"), Ok(None)));
    }

    #[test]
    fn test_basic() {
        let file = prepare_file(
            r#"
a: 1
b: 1.2
c: xd
foo:
  bar: xd
  xd: c
  a:
    a: 1
"#,
        );
        let config = YamlConfig::new(file.as_ref().to_path_buf()).unwrap();
        assert!(matches!(config.get_table("foo"), Ok(Some(..))));
        assert!(matches!(config.get_table("foo.a"), Ok(Some(..))));
        assert!(matches!(config.get_str("c"), Ok(Some("xd"))));
        assert!(matches!(config.get_f64("b"), Ok(Some(1.2))));
        assert!(matches!(config.get_i64("a"), Ok(Some(1))));
        assert!(matches!(config.get_i64("foo.a.a"), Ok(Some(1))));
        assert_eq!(
            config.get_i64("foo.xd.a").unwrap_err().to_string(),
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
            config.get_f64("foo.xd.a").unwrap_err().to_string(),
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

a: 1
b: 1.2
c: xd
d: dx
foo:
  bar: xd
  xd: c
  a:
    a: 1
"#,
        );
        let mut config = YamlConfig::new(file.as_ref().to_path_buf()).unwrap();
        config.set_bool("a", true).unwrap();
        config.set_i64("b", 2137).unwrap();
        config.set_table("c", Mapping::new()).unwrap();
        config.set_array("foo", Sequence::new()).unwrap();
        config
            .set_table("e.bar.xd", Mapping::from_iter([("a".into(), 1.into())]))
            .unwrap();
        let err = config.set_i64("foo.a", 1).unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo` expected a table, not an array",
                file.path().display()
            )
        );
        assert_eq!(
            config.to_string(),
            "\
a: true
b: 2137
c: {}
d: dx
foo: []
e:
  bar:
    xd:
      a: 1
"
        );
    }

    #[test]
    fn multiple_docs() {
        let file = prepare_file(
            r#"
---
a: 1
...
---
b: 1
...
"#,
        );
        let err = YamlConfig::new(file.as_ref().to_path_buf()).unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "when trying to parse a user config at `{}` into the YAML value
deserializing from YAML containing more than one document is not supported",
                file.path().display()
            )
        )
    }
}
