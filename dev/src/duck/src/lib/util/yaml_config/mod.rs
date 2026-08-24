//! Implementation of traversing YAML documents, and getting/setting values at dotted keys.
use std::path::{Path, PathBuf};
use std::{fmt, io};

use serde::Deserialize;
use serde_yaml_ng::{Mapping, Sequence, Value, from_str, to_string};
use tracing::debug;

use super::DescriptionWithAnArticle;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err};

mod de;

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
        #[tracing::instrument(skip_all)]
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
        #[tracing::instrument(skip_all)]
        pub fn $name(&mut self, key: &str, value: $value) -> QuackResult<()> {
            let value: Value = value.into();
            self.set(key, value)
        }
    };
}

#[derive(Default)]
/// YAML config manager.
pub struct YamlConfig {
    content: Mapping,
    source: Option<PathBuf>,
}

impl fmt::Debug for YamlConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("YamlConfig")
            .field("where", &self.source)
            .finish_non_exhaustive()
    }
}

impl YamlConfig {
    /// Create a new [`YamlConfig`] from the YAML file at `path`.
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        debug!(path = %path.display(), "parsing YAML config");
        let content = match path.as_path().read_to_string() {
            Ok(string) => string,
            Err(e) => {
                if let Some(err) = e.downcast_ref_in_chain::<io::Error>()
                    && err.kind() == io::ErrorKind::NotFound
                {
                    debug!(
                        path = %path.display(),
                        "missing config",
                    );
                    return Ok(Self::default());
                } else {
                    return Err(e).context(format!(
                        "when trying to read a user config at `{}`",
                        path.display()
                    ));
                }
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

    fn get_location_description(&self) -> impl fmt::Display {
        self.source
            .as_ref()
            .map(|buf| buf.display())
            .unwrap_or_else(|| Path::new("<default-config>").display()) // It's dyn-hack.
    }

    #[track_caller]
    /// Get the value from the dotted key.
    fn _get(&self, key: &str) -> QuackResult<Option<&Value>> {
        debug!(%key, where = %self.get_location_description());
        if key.is_empty() {
            qp_bail_internal!("empty key in `_get`")
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
                    table = %part,
                    chain = ?parts[0..=i],
                    "there is no table in chain",
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
        debug!(%key, ?value, where = %self.get_location_description());
        if key.is_empty() {
            qp_bail_internal!("empty key in `_set`")
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

    /// Deserialize a value at the dotted key.
    pub fn deserialize<'de, T: Deserialize<'de>>(&'de self, key: &str) -> QuackResult<T> {
        let deserializer = de::YamlDeserializer { config: self, key };
        T::deserialize(deserializer).with_context(|| self.make_location_error())
    }

    /// Deserialize an optional value at the dotted key.
    pub fn deserialize_optional<'de, T: Deserialize<'de>>(
        &'de self,
        key: &str,
    ) -> QuackResult<Option<T>> {
        self.deserialize::<Option<T>>(key)
    }

    /// Deserialize an optional value at the dotted key, or return the default.
    pub fn deserialize_optional_or_default<'de, T: Deserialize<'de> + Default>(
        &'de self,
        key: &str,
    ) -> QuackResult<T> {
        self.deserialize::<Option<T>>(key)
            .map(|value| value.unwrap_or_default())
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
    use tempfile::NamedTempFile;

    use super::*;

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

    #[test]
    fn deserializer_tests() {
        // cSpell:disable
        use serde::de;

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
        let config = YamlConfig::new(file.as_ref().to_path_buf()).unwrap();
        let a: i32 = config.deserialize("a").unwrap();
        assert_eq!(a, 1);
        let b: f64 = config.deserialize("b").unwrap();
        assert_eq!(b, 1.2);
        let empty = config
            .deserialize_optional::<Vec<i32>>("nonexistentkey")
            .unwrap();
        assert!(empty.is_none());

        #[derive(Deserialize, Eq, PartialEq, Debug)]
        struct Foo {
            bar: String,
            xd: Option<String>,
            nonexistent: Option<i32>,
        }

        #[derive(Deserialize, Eq, PartialEq, Debug)]
        struct A {
            a: i32,
        }

        #[derive(Deserialize, Eq, PartialEq, Debug)]
        struct FooWithA {
            bar: String,
            xd: Option<String>,
            nonexistent: Option<i32>,
            a: A,
        }

        let foo: Foo = config.deserialize("foo").unwrap();
        assert_eq!(
            foo,
            Foo {
                bar: "xd".into(),
                xd: Some("c".into()),
                nonexistent: None
            }
        );

        let a: A = config.deserialize("foo.a").unwrap();
        assert_eq!(a, A { a: 1 });

        let fooa: FooWithA = config.deserialize("foo").unwrap();
        assert_eq!(
            fooa,
            FooWithA {
                bar: "xd".into(),
                xd: Some("c".into()),
                nonexistent: None,
                a: A { a: 1 },
            }
        );

        let missing = config.deserialize::<i32>("nonexistentkey").unwrap_err();
        assert_eq!(
            missing.to_string(),
            format!(
                "\
when parsing the configuration at `{}`
missing key `nonexistentkey`",
                file.path().display()
            )
        );

        let maybefooa = config.deserialize_optional::<FooWithA>("foo").unwrap();
        assert_eq!(maybefooa, Some(fooa));

        let err = config.deserialize::<FooWithA>("a").unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "\
when parsing the configuration at `{}`
invalid type: integer `1`, expected struct FooWithA",
                file.path().display()
            )
        );

        #[derive(Eq, PartialEq, Debug)]
        enum IntOrString {
            Int(i32),
            String(String),
        }

        impl<'de> de::Deserialize<'de> for IntOrString {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: serde::Deserializer<'de>,
            {
                serde_untagged::UntaggedEnumVisitor::new()
                    .expecting("an int or a string")
                    .i32(|x| Ok(Self::Int(x)))
                    .string(|str| Ok(Self::String(str.into())))
                    .deserialize(deserializer)
            }
        }

        let a: IntOrString = config.deserialize("a").unwrap();
        assert_eq!(a, IntOrString::Int(1));
        let c: IntOrString = config.deserialize("c").unwrap();
        assert_eq!(c, IntOrString::String("xd".into()));
        let err = config.deserialize::<IntOrString>("foo").unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "\
when parsing the configuration at `{}`
invalid type: map, expected an int or a string",
                file.path().display()
            )
        );

        let err = config
            .deserialize::<IntOrString>("nonexistentkey")
            .unwrap_err();
        assert_eq!(
            err.to_string(),
            format!(
                "\
when parsing the configuration at `{}`
missing key `nonexistentkey`",
                file.path().display()
            )
        );

        let none = config
            .deserialize::<Option<IntOrString>>("nonexistentkey")
            .unwrap();
        assert!(none.is_none());

        let none = config
            .deserialize_optional::<IntOrString>("nonexistentkey")
            .unwrap();
        assert!(none.is_none());
        // cSpell:enable
    }
}
