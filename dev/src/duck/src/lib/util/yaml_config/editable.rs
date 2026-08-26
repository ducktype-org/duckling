//use std::convert::identity;
use std::fmt::{self, Debug};
use std::io;
use std::path::{Path, PathBuf};

use tracing::debug;
use yaml_edit::path::{PathSegment, parse_path};
use yaml_edit::{
    AsYaml, Mapping, MappingBuilder, Sequence, SequenceBuilder, YamlError, YamlFile, YamlNode,
};

use crate::util::DescriptionWithAnArticle;
use crate::{
    QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err, qp_internal,
};

/// Helper trait for getting values from [`YamlConfig`].
trait ToValue {
    fn _to_mapping(self) -> Option<Mapping>;
    fn _to_sequence(self) -> Option<Sequence>;
    fn _to_str(self) -> Option<String>;
    fn _to_i64(self) -> Option<i64>;
    fn _to_f64(self) -> Option<f64>;
    fn _to_bool(self) -> Option<bool>;
}

impl ToValue for YamlNode {
    fn _to_mapping(self) -> Option<Mapping> {
        if let Self::Mapping(x) = self {
            Some(x)
        } else {
            None
        }
    }

    fn _to_sequence(self) -> Option<Sequence> {
        if let Self::Sequence(x) = self {
            Some(x)
        } else {
            None
        }
    }

    fn _to_str(self) -> Option<String> {
        if let Self::Scalar(x) = self {
            Some(x.as_string())
        } else {
            None
        }
    }

    fn _to_i64(self) -> Option<i64> {
        self.to_i64()
    }

    fn _to_f64(self) -> Option<f64> {
        self.to_f64()
    }

    fn _to_bool(self) -> Option<bool> {
        self.to_bool()
    }
}

impl DescriptionWithAnArticle for YamlNode {
    fn desc_with_article(&self) -> &'static str {
        match self {
            Self::Scalar(..) => "a value",
            Self::Sequence(..) => "an array",
            Self::Mapping(..) => "a table",
            Self::TaggedNode(..) => "a tagged value",
            Self::Alias(..) => "an alias",
        }
    }
}

macro_rules! delegate_getter {
    (
        FunctionName: $name:ident,
        ReturnType: $ret:ty,
        HumanType: $human_type:literal,
        CastFunctionName: $yaml_value_fn:ident $(,)?
    ) => {
        #[doc = concat!("Get [`", stringify!($ret), "`] at the dotted key.")]
        #[tracing::instrument(skip_all)]
        pub fn $name(&self, key: &str) -> QuackResult<Option<$ret>> {
            let value = self.get(key)?;
            let Some(value) = value else {
                return Ok(None);
            };
            let desc = value.desc_with_article();
            match value.$yaml_value_fn() {
                Some(x) => Ok(Some(x)),
                None => Err(qp_err!("{}", self.make_location_error())).context(format!(
                    "the key `{key}` expects {}, not {desc}",
                    $human_type
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
            self.set(key, value)
        }
    };
}

#[derive(Default)]
/// YAML config manager.
pub struct YamlConfig {
    content: YamlFile,
    source: Option<PathBuf>,
}

impl fmt::Debug for YamlConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("YamlConfig")
            .field("where", &self.source)
            .finish_non_exhaustive()
    }
}

impl fmt::Display for YamlConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let content = &self.content.to_string();
        write!(f, "{}", content)
    }
}

impl YamlConfig {
    /// Create a new [`YamlConfig`] from the YAML file at `path`.
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        debug!(path = %path.display(), "parsing YAML config");
        let content = match YamlFile::from_path(&path) {
            Ok(file) => Ok(file),
            Err(YamlError::Io(io_err)) => {
                if io_err.kind() == io::ErrorKind::NotFound {
                    debug!(
                        path = %path.display(),
                        "missing config",
                    );
                    return Ok(Self::default());
                } else {
                    Err(qp_err!(io_err))
                }
            }
            Err(e) => Err(qp_err!(e)),
        }
        .with_context(|| format!("when trying to read a user config at `{}`", path.display()))?;
        let result = Self {
            content,
            source: Some(path),
        };
        // Make sure `as_mapping` will work in the future.
        result.as_mapping().with_context(|| result.make_location_error())?;
        Ok(result)
    }

    /// Get the underlying [`Mapping`] of this [`YamlConfig`].
    fn as_mapping(&self) -> QuackResult<Mapping> {
        if self.content.documents().count() > 1 {
            qp_bail!("file contains multiple YAML documents which is not supported")
        }
        self.content
            .document()
            .with_context(|| format!("no document found inside `{}`", self.make_location_error()))?
            .as_mapping()
            .with_context(|| {
                format!(
                    "document found inside `{}` is not a mapping",
                    self.make_location_error()
                )
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

    /// Get the value at the dotted key.
    fn _get(&self, key: &str) -> QuackResult<Option<YamlNode>> {
        debug!(%key, where = %self.get_location_description());
        if key.is_empty() {
            qp_bail_internal!("empty key in `_get`")
        }
        let parts = get_parts_from_key(key)?;
        let [ref parts @ .., ref last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so there should return at least one element"
            )
        };
        let mut current: Mapping = self.as_mapping()?;
        for (i, part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, part));
            }
            let Some(next) = current.get(part) else {
                debug!(
                    table = %part,
                    chain = ?parts[0..=i],
                    "there is no table in chain",
                );
                return Ok(None);
            };
            let YamlNode::Mapping(next) = next else {
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
    fn get(&self, key: &str) -> QuackResult<Option<YamlNode>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    #[track_caller]
    /// Set the value at the dotted key.
    fn _set(&mut self, key: &str, value: impl AsYaml + Debug) -> QuackResult<()> {
        debug!(%key, ?value, where = %self.get_location_description());
        if key.is_empty() {
            qp_bail_internal!("empty key in `_set`")
        }
        let parts = get_parts_from_key(key)?;
        let [ref parts @ .., ref last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so split should return at least one element"
            )
        };
        let mut current = self.as_mapping()?;
        for (i, part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, key));
            }

            if !current.contains_key(part) {
                current.set(part, new_mapping());
            }
            let next = current.get(part).with_context_internal(|| {
                format!("inserted key {part} but there is no entry for it in {current:?}")
            })?;
            let YamlNode::Mapping(next) = next else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {}",
                    parts[0..=i].join("."),
                    next.desc_with_article(),
                )
            };
            current = next;
        }
        current.set(last, value);
        Ok(())
    }

    /// Convenient wrapper around [`_set`](Self::_set).
    fn set(&mut self, key: &str, value: impl AsYaml + Debug) -> QuackResult<()> {
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
        ReturnType: String,
        HumanType: "a string",
        CastFunctionName: _to_str,
    }

    delegate_getter! {
        FunctionName: get_array,
        ReturnType: Sequence,
        HumanType: "an array",
        CastFunctionName: _to_sequence,
    }

    delegate_getter! {
        FunctionName: get_table,
        ReturnType: Mapping,
        HumanType: "a table",
        CastFunctionName: _to_mapping,
    }

    delegate_getter! {
        FunctionName: get_i64,
        ReturnType: i64,
        HumanType: "an int",
        CastFunctionName: _to_i64,
    }

    delegate_getter! {
        FunctionName: get_f64,
        ReturnType: f64,
        HumanType: "a float",
        CastFunctionName: _to_f64,
    }

    delegate_getter! {
        FunctionName: get_bool,
        ReturnType: bool,
        HumanType: "a boolean",
        CastFunctionName: _to_bool,
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
}

/// Construct an empty [`Mapping`].
pub fn new_mapping() -> Mapping {
    MappingBuilder::new()
        .build_document()
        .as_mapping()
        .expect("error in `yaml-edit`, `MappingBuilder` should build a mapping")
}

/// Construct an empty [`Sequence`].
pub fn new_sequence() -> Sequence {
    SequenceBuilder::new()
        .build_document()
        .as_sequence()
        .expect("error in `yaml-edit`, `SequenceBuilder` should build a mapping")
}

/// Divide dotted key into parts.
fn get_parts_from_key(key: &str) -> QuackResult<Vec<String>> {
    parse_path(key)
        .into_iter()
        .map(|part| match part {
            PathSegment::Key(key) => Ok(key),
            PathSegment::Index(_) => Err(qp_internal!("index into sequence found in key {key}")),
        })
        .collect()
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
        // This errors
        // !TODO: decide what the behaviour should be.
        /*
        let file = prepare_file("");
        let config = YamlConfig::new(file.as_ref().to_path_buf()).unwrap();
        assert!(matches!(config.get_table("a"), Ok(None)));
        assert!(matches!(config.get_array("a"), Ok(None)));
        assert!(matches!(config.get_str("a"), Ok(None)));
        assert!(matches!(config.get_f64("a"), Ok(None)));
        assert!(matches!(config.get_bool("a"), Ok(None)));*/
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
        assert_eq!(config.get_str("c").unwrap(), Some("xd".to_string()));
        assert_eq!(config.get_f64("b").unwrap(), Some(1.2));
        assert_eq!(config.get_i64("a").unwrap(), Some(1));
        assert_eq!(config.get_i64("foo.a.a").unwrap(), Some(1));
        assert_eq!(
            config.get_i64("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_table("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_array("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_bool("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_f64("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
                file.path().display()
            )
        );
        assert_eq!(
            config.get_str("foo.xd.a").unwrap_err().to_string(),
            format!(
                "when parsing the configuration at `{}`\nin the chain `foo.xd` expected a table, not a value",
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
                .as_scalar()
                .unwrap()
                .as_string(),
            "c"
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
        // Setting empty mapping does not work :( (bug in yaml-edit).
        // config.set_table("c", new_mapping()).unwrap();
        let seq = new_sequence();
        // Setting empty sequence does not wort :( (bug in yaml-edit).
        seq.push("seq");
        config.set_array("foo", seq).unwrap();
        let map = new_mapping();
        map.set("a", 1);
        config.set_table("e.bar.xd", map).unwrap();
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
            "
a: true
b: 2137
c: xd
d: dx
foo:
    - seq
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
                "when parsing the configuration at `{}`
file contains multiple YAML documents which is not supported",
                file.path().display()
            )
        )
    }

    #[test]
    fn editability() {
        let comment = prepare_file(
            r#"
# comment
foo: 1
"#,
        );
        let mut config = YamlConfig::new(comment.as_ref().to_path_buf()).unwrap();
        config.set("bar", 2).unwrap();
        assert_eq!(
            config.content.to_string(),
            "
# comment
foo: 1
bar: 2
"
        );

        let weird_tabulation = prepare_file(
            r#"
foo:
   bar: 1
"#,
        );
        let mut config = YamlConfig::new(weird_tabulation.as_ref().to_path_buf()).unwrap();
        config.set("xd", 2).unwrap();
        assert_eq!(
            config.content.to_string(),
            "
foo:
   bar: 1
xd: 2
"
        );

        // `set` with flow style does not work in yaml-edit :(
        /*let style = prepare_file(
            r#"
foo: {bar: 1}
"#,
        );
        let mut config = YamlConfig::new(style.as_ref().to_path_buf()).unwrap();
        config.set("foo.xd", 2).unwrap();
        assert_eq!(
            config.content.to_string(),
            "
foo: {bar: 1, xd: 2}
"
        );*/
    }
}
