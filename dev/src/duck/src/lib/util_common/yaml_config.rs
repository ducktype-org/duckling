// We have to add it, because saphyr (YAML library) exposes this type explicitly,
// so we have to pull it in order to use it.
use ordered_float::OrderedFloat;
use paste::item;
use saphyr::{
    LoadableYamlNode, Mapping, MappingOwned, Scalar, ScalarOwned, SequenceOwned, Yaml, YamlEmitter,
    YamlOwned,
};
use std::{
    fmt,
    io::ErrorKind,
    path::{Path, PathBuf},
};
use tracing::debug;

use crate::{
    QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err, qp_internal,
    util_common::path_ops_ext::PathOpsExt,
};

use super::DescriptionWithAnArticle;

impl DescriptionWithAnArticle for Yaml<'_> {
    fn desc_with_article(&self) -> &'static str {
        match self {
            Self::Representation(..) => "an internal yaml representation",
            Self::Value(Scalar::Null) => "a null",
            Self::Value(Scalar::Boolean(..)) => "a boolean",
            Self::Value(Scalar::Integer(..)) => "an integer",
            Self::Value(Scalar::FloatingPoint(..)) => "a float",
            Self::Value(Scalar::String(..)) => "a string",
            Self::Sequence(..) => "an array",
            Self::Mapping(..) => "a table",
            Self::Tagged(..) => "a tagged value",
            Self::Alias(..) => "an alias",
            Self::BadValue => "an invalid value",
        }
    }
}

impl DescriptionWithAnArticle for YamlOwned {
    fn desc_with_article(&self) -> &'static str {
        Yaml::from(self).desc_with_article()
    }
}

trait ToYamlOwned {
    fn to_owned_yaml(&self) -> YamlOwned;
}

impl ToYamlOwned for str {
    fn to_owned_yaml(&self) -> YamlOwned {
        YamlOwned::Value(ScalarOwned::String(self.to_owned()))
    }
}

macro_rules! delegate_getter {
    (
        $(
            $name:ident => $yaml_value_fn:ident -> $ret:ty: $human_type:literal $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<get_ $name>](&self, key: &str) -> QuackResult<Option<$ret>> {
                    let value = self.get(key)?;
                    let Some(value) = value else {
                        return Ok(None);
                    };
                    match value.[<as_ $yaml_value_fn>]() {
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
            $name:ident => $yaml_value_enum:ident -> $value:ty $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<set_ $name>](&mut self, key: &str, value: $value) -> QuackResult<()> {
                    let value = YamlOwned::$yaml_value_enum(value);
                    self.set(key, value)
                }
            )*
        }
    };
}

macro_rules! delegate_scalar_setter {
    (
        $(
            $name:ident => $yaml_value_enum:ident -> $value:ty $(,)?
        ),*
    ) => {
        item! {
            $(
                pub fn [<set_ $name>](&mut self, key: &str, value: $value) -> QuackResult<()> {
                    let value = YamlOwned::Value(ScalarOwned::$yaml_value_enum(value));
                    self.set(key, value)
                }
            )*
        }
    };
}

#[derive(Default, Debug)]
pub struct YamlConfig {
    content: MappingOwned,
    source: Option<PathBuf>,
}

impl YamlConfig {
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
        let content = YamlOwned::load_from_str(&content).with_context(|| {
            format!(
                "when trying to parse a user config at `{}` into the YAML node",
                path.display()
            )
        })?;
        // Make empty files work.
        if content.is_empty() {
            return Ok(Self::default());
        }
        let [content] = content.try_into().map_err(|docs: Vec<YamlOwned>| {
            if docs.len() < 2 {
                return qp_internal!("it should've been guarded by `.is_empty()` and LHS");
            }
            qp_err!(
                "user config at `{}` has multiple ({}) YAML documents, which is not supported",
                path.display(),
                docs.len()
            )
        })?;
        let content = content
            .into_mapping()
            .ok_or_else(|| qp_err!("user config at `{}` is not a YAML table", path.display()))?;
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
    fn _get(&self, key: &str) -> QuackResult<Option<&YamlOwned>> {
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
        let mut current: &MappingOwned = &self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, key));
            }
            let Some(next) = current.get(&part.to_owned_yaml()) else {
                debug!(
                    "there is no table `[{part}]` in the chain `{}`",
                    parts[0..=i].join("."),
                );
                return Ok(None);
            };
            let YamlOwned::Mapping(next) = next else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {}",
                    parts[0..=i].join("."),
                    next.desc_with_article(),
                )
            };
            current = next;
        }
        Ok(current.get(&last.to_owned_yaml()))
    }

    fn get(&self, key: &str) -> QuackResult<Option<&YamlOwned>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    #[track_caller]
    fn _set(&mut self, key: &str, value: YamlOwned) -> QuackResult<()> {
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
                .entry(part.to_owned_yaml())
                .or_insert_with(|| YamlOwned::Mapping(Default::default()));

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
        let _ = current.insert(last.to_owned_yaml(), value);
        Ok(())
    }

    fn set(&mut self, key: &str, value: YamlOwned) -> QuackResult<()> {
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
        array => sequence -> &SequenceOwned: "an array",
        table => mapping -> &MappingOwned: "a table",
        int => integer -> i64: "an integer",
        float => floating_point -> f64: "a float",
        bool => bool -> bool: "a boolean",
    }

    delegate_setter! {
        array => Sequence -> SequenceOwned,
        table => Mapping -> MappingOwned
    }

    delegate_scalar_setter! {
        bool => Boolean -> bool,
        int => Integer -> i64,
        str => String -> String,
    }

    // We set this one manually, because YAML API is awful.
    pub fn set_float(&mut self, key: &str, value: f64) -> QuackResult<()> {
        let value = YamlOwned::Value(ScalarOwned::FloatingPoint(OrderedFloat(value)));
        self.set(key, value)
    }

    pub fn get_root_table(&self) -> &MappingOwned {
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

impl fmt::Display for YamlConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut emitter = YamlEmitter::new(f);
        // Hack around two things:
        //   1. MappingOwned doesn't implement Display
        //   2. `.dump` takes `&Yaml<'_>` (COW version of YamlOwned), so we have to convert
        //      types.
        let yaml = Yaml::Mapping(
            self.content
                .iter()
                .map(|(key, value)| (key.into(), value.into()))
                .collect::<Mapping>(),
        );
        emitter.dump(&yaml).map_err(|_| fmt::Error)
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
        assert!(matches!(config.get_float("a"), Ok(None)));
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
                .get(&"xd".to_owned_yaml())
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
        config.set_int("b", 2137).unwrap();
        config.set_table("c", MappingOwned::new()).unwrap();
        config.set_array("foo", SequenceOwned::new()).unwrap();
        config
            .set_table(
                "e.bar.xd",
                MappingOwned::from_iter([(
                    "a".to_owned_yaml(),
                    YamlOwned::Value(ScalarOwned::Integer(1)),
                )]),
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
            r#"---
d: dx
a: true
b: 2137
c: {}
e:
  bar:
    xd:
      a: 1
foo: []"#
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
                "user config at `{}` has multiple (2) YAML documents, which is not supported",
                file.path().display()
            )
        )
    }
}
