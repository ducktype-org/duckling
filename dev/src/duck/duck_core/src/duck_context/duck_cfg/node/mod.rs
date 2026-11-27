// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A single config node.

use std::collections::HashMap;
use std::path::PathBuf;
use std::{fmt, io};

use serde::Deserialize;
use tracing::{debug, trace};

use crate::util::filesystem::read_from_path_to_string;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err};

pub mod de;
pub use de::Sourced;

#[derive(Debug, Clone, Eq, PartialEq, Hash, Default)]
/// Source of the [`Node`].
pub enum Source {
    /// This is a builtin value.
    #[default]
    Builtin,
    /// We've loaded this value from the given path.
    Path(PathBuf),
}

impl fmt::Display for Source {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Builtin => write!(f, "builtin source"),
            Self::Path(path_buf) => write!(f, "file at `{}`", path_buf.display()),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq)]
/// A node in a user's config file.
pub enum Node {
    Integer(i64, Source),
    Boolean(bool, Source),
    String(String, Source),
    Seq(Vec<Node>, Source),
    Map(HashMap<String, Node>, Source),
}

impl Default for Node {
    fn default() -> Self {
        Self::Map(Default::default(), Default::default())
    }
}

impl Node {
    /// Try to parse a [`Node`] from the given path.
    ///
    /// If there was no file, returns [`None`].
    ///
    /// Always returns [`Node::Map`].
    pub fn from_path(path: PathBuf) -> QuackResult<Option<Self>> {
        debug!("parsing the user config at `{}`", path.display());
        let content = match read_from_path_to_string(&path) {
            Ok(string) => string,
            Err(e) => {
                if let Some(err) = e.downcast_ref_in_chain::<io::Error>()
                    && err.kind() == io::ErrorKind::NotFound
                {
                    debug!("no user config at `{}`", path.display());
                    return Ok(None);
                } else {
                    return Err(e).context(format!(
                        "failed to read the user config at `{}`",
                        path.display()
                    ));
                }
            }
        };

        let content: serde_yaml_ng::Value =
            serde_yaml_ng::from_str(&content).with_context(|| {
                format!(
                    "failed to parse the user config at `{}` into a YAML value",
                    path.display()
                )
            })?;
        match content {
            serde_yaml_ng::Value::Mapping(..) => {}
            // Make empty files work.
            serde_yaml_ng::Value::Null => return Ok(None),
            _ => qp_bail!("user config at `{}` is not a YAML table", path.display()),
        };
        let source = Source::Path(path);
        Self::from_yaml(content, source.clone())
            .with_context(|| format!("failed to parse the configuration from the {source}"))
    }

    /// Create a [`Node`] from a YAML [`Value`].
    ///
    /// [`Value`]: serde_yaml_ng::Value
    fn from_yaml(value: serde_yaml_ng::Value, source: Source) -> QuackResult<Option<Self>> {
        match value {
            serde_yaml_ng::Value::Null => Ok(None),
            serde_yaml_ng::Value::Bool(b) => Ok(Some(Self::Boolean(b, source))),
            serde_yaml_ng::Value::Number(number) => {
                if let Some(signed) = number.as_i64() {
                    return Ok(Some(Self::Integer(signed, source)));
                }
                if let Some(unsigned) = number.as_u64()
                    && let Ok(signed) = i64::try_from(unsigned)
                {
                    return Ok(Some(Self::Integer(signed, source)));
                }
                let ty = if number.is_u64() {
                    "an unsigned integer"
                } else if number.is_f64() {
                    "a float"
                } else {
                    unreachable!("unknown YAML number type `{number:?}` at `{source}`")
                };
                qp_bail!("unsupported YAML number type {ty} at {source}")
            }
            serde_yaml_ng::Value::String(s) => Ok(Some(Self::String(s, source))),
            serde_yaml_ng::Value::Sequence(values) => {
                let list = values
                    .into_iter()
                    .flat_map(|value| Self::from_yaml(value, source.clone()).transpose())
                    .collect::<QuackResult<Vec<_>>>()?;
                Ok(Some(Self::Seq(list, source)))
            }
            serde_yaml_ng::Value::Mapping(mapping) => {
                let mapping = mapping
                    .into_iter()
                    .map(|(key, value)| {
                        let serde_yaml_ng::Value::String(key) = key else {
                            qp_bail!(
                                "unsupported key `{}` in a YAML mapping at {source}",
                                describe_yaml(&key)
                            )
                        };
                        let value = Self::from_yaml(value, source.clone())?;
                        Ok((key, value))
                    })
                    .filter_map(|result| {
                        let (key, value) = match result {
                            Ok((key, value)) => (key, value),
                            Err(e) => return Some(Err(e)),
                        };
                        let value = value?;
                        Some(Ok((key, value)))
                    })
                    .collect::<QuackResult<HashMap<_, _>>>()?;
                Ok(Some(Self::Map(mapping, source)))
            }
            serde_yaml_ng::Value::Tagged(..) => qp_bail!("unsupported YAML tag at {source}"),
        }
    }

    /// Get the [`Source`] of this [`Node`].
    pub fn source(&self) -> &Source {
        match self {
            Node::Integer(_, source)
            | Node::Boolean(_, source)
            | Node::String(_, source)
            | Node::Seq(_, source)
            | Node::Map(_, source) => source,
        }
    }

    /// Try to deserialize a `T` from the given `key`.
    ///
    /// Returns [`None`] if the key was not found.
    ///
    /// Use [`deserialize_or_default`] if you want a fallback.
    ///
    /// [`deserialize_or_default`]: Self::deserialize_or_default
    pub fn deserialize<'de, T>(&'de self, key: &str) -> QuackResult<Option<T>>
    where
        T: Deserialize<'de>,
    {
        let node = match self.get(key)? {
            Some(node) => node,
            None => return Ok(None),
        };

        T::deserialize(de::NodeDeserializer { node })
            // @TODO: #4243 Move this to `de.rs` (and make more precise).
            // Also use internal errors when appropriate.
            .with_context(|| {
                format!(
                    "failed to parse the configuration from the {}",
                    self.source()
                )
            })
            .map(Some)
    }

    /// Try to deserialize `T` from the given `key`.
    ///
    /// If `key` was not found, returns `Ok(Default::default())`.
    pub fn deserialize_or_default<'de, T>(&'de self, key: &str) -> QuackResult<T>
    where
        T: Deserialize<'de> + Default,
    {
        let value = self.deserialize(key)?.unwrap_or_default();
        Ok(value)
    }

    #[track_caller]
    /// Get the value from the dotted key.
    fn get(&self, key: &str) -> QuackResult<Option<&Node>> {
        trace!(%key, ?self);
        if key.is_empty() {
            qp_bail_internal!("empty key in `get`")
        }
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so split should return at least one element"
            )
        };
        let mut current = match self {
            Node::Map(map, _) => map,
            _ => unreachable!("Node {self:?} is not a map"),
        };
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(make_empty_key_fragment_error(i, key));
            }
            let Some(next) = current.get(part) else {
                debug!(
                    table = %part,
                    chain = ?parts[0..=i],
                    "there is no table in chain",
                );
                return Ok(None);
            };
            let Node::Map(next, _) = next else {
                qp_bail!(
                    "in the chain `{}` expected a table, not {} at {}",
                    parts[0..=i].join("."),
                    describe_node_with_article(next),
                    next.source()
                )
            };
            current = next;
        }
        Ok(current.get(last))
    }

    /// Try to cast this [`Node`] to an [`i64`].
    pub fn as_i64(&self) -> QuackResult<i64> {
        let Self::Integer(value, _) = self else {
            qp_bail!(
                "expected an integer not {}",
                describe_node_with_article(self)
            );
        };
        Ok(*value)
    }

    /// Try to cast this [`Node`] to a [`bool`].
    pub fn as_bool(&self) -> QuackResult<bool> {
        let Self::Boolean(value, _) = self else {
            qp_bail!(
                "expected a boolean not {}",
                describe_node_with_article(self)
            );
        };
        Ok(*value)
    }

    /// Try to cast this [`Node`] to a [`str`].
    pub fn as_str(&self) -> QuackResult<&str> {
        let Self::String(string, _) = self else {
            qp_bail!("expected a string not {}", describe_node_with_article(self));
        };
        Ok(string)
    }

    /// Try to cast this [`Node`] to a slice.
    pub fn as_seq(&self) -> QuackResult<&[Node]> {
        let Self::Seq(slice, _) = self else {
            qp_bail!(
                "expected a sequence not {}",
                describe_node_with_article(self)
            );
        };
        Ok(slice)
    }

    /// Try to cast this [`Node`] to a [`HashMap`].
    pub fn as_map(&self) -> QuackResult<&HashMap<String, Node>> {
        let Self::Map(map, _) = self else {
            qp_bail!(
                "expected a mapping not {}",
                describe_node_with_article(self)
            );
        };
        Ok(map)
    }

    /// Try to cast this [`Node`] to a [`str`].
    pub fn as_str_mut(&mut self) -> QuackResult<&mut String> {
        let Self::String(string, _) = self else {
            qp_bail!("expected a string not {}", describe_node_with_article(self));
        };
        Ok(string)
    }

    /// Try to cast this [`Node`] to a slice.
    pub fn as_seq_mut(&mut self) -> QuackResult<&mut Vec<Node>> {
        let Self::Seq(slice, _) = self else {
            qp_bail!(
                "expected a sequence not {}",
                describe_node_with_article(self)
            );
        };
        Ok(slice)
    }

    /// Try to cast this [`Node`] to a [`HashMap`].
    pub fn as_map_mut(&mut self) -> QuackResult<&mut HashMap<String, Node>> {
        let Self::Map(map, _) = self else {
            qp_bail!(
                "expected a mapping not {}",
                describe_node_with_article(self)
            );
        };
        Ok(map)
    }

    /// Try to cast this [`Node`] to a [`str`].
    pub fn to_string(self) -> QuackResult<String> {
        let Self::String(string, _) = self else {
            qp_bail!(
                "expected a string not {}",
                describe_node_with_article(&self)
            );
        };
        Ok(string)
    }

    /// Try to cast this [`Node`] to a slice.
    pub fn to_seq(self) -> QuackResult<Vec<Node>> {
        let Self::Seq(slice, _) = self else {
            qp_bail!(
                "expected a sequence not {}",
                describe_node_with_article(&self)
            );
        };
        Ok(slice)
    }

    /// Try to cast this [`Node`] to a [`HashMap`].
    pub fn to_map(self) -> QuackResult<HashMap<String, Node>> {
        let Self::Map(map, _) = self else {
            qp_bail!(
                "expected a mapping not {}",
                describe_node_with_article(&self)
            );
        };
        Ok(map)
    }
}

/// Describe [`Value`] without an article.
///
/// [`Value`]: serde_yaml_ng::Value
fn describe_yaml(value: &serde_yaml_ng::Value) -> &'static str {
    match value {
        serde_yaml_ng::Value::Null => "null",
        serde_yaml_ng::Value::Bool(..) => "boolean",
        serde_yaml_ng::Value::Number(n) => {
            if n.is_f64() {
                "float"
            } else if n.is_u64() {
                "positive integer"
            } else {
                "integer"
            }
        }
        serde_yaml_ng::Value::String(..) => "string",
        serde_yaml_ng::Value::Sequence(..) => "array",
        serde_yaml_ng::Value::Mapping(..) => "table",
        serde_yaml_ng::Value::Tagged(..) => "tagged value",
    }
}

/// Describe [`Node`] with a correct article.
fn describe_node_with_article(node: &Node) -> &'static str {
    match node {
        Node::Integer(..) => "an integer",
        Node::Boolean(..) => "a boolean",
        Node::String(..) => "a string",
        Node::Seq(..) => "a sequence",
        Node::Map(..) => "a map",
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    const SOURCE: Source = Source::Builtin;

    fn make_node(content: &str) -> Node {
        let value: serde_yaml_ng::Value = serde_yaml_ng::from_str(content).unwrap();
        assert!(value.is_mapping(), "{value:?} not a mapping");
        Node::from_yaml(value, SOURCE).unwrap().unwrap()
    }

    #[test]
    fn test_empty() {
        let node = Node::default();
        let value = node.get("a").unwrap();
        assert_eq!(value, None);
    }

    #[test]
    fn test_basic() {
        let node = make_node(
            r#"
a: 1
c: xd
foo:
  bar: xd
  xd: c
  a:
    a: 1
"#,
        );
        let a = node.get("foo.a.a").unwrap().unwrap();
        assert_eq!(a, &Node::Integer(1, SOURCE));
        let c = node.get("c").unwrap().unwrap();
        assert_eq!(c, &Node::String("xd".into(), SOURCE));
    }

    #[test]
    fn deserializer_tests() {
        // cSpell:disable
        use serde::de;

        let node = make_node(
            r#"

a: 1
c: xd
d: dx
foo:
  bar: xd
  xd: c
  a:
    a: 1
"#,
        );
        let a = node.deserialize::<i32>("a").unwrap();
        assert_eq!(a, Some(1));
        let empty = node.deserialize::<Vec<i32>>("nonexistentkey").unwrap();
        assert_eq!(empty, None);

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

        let foo = node.deserialize::<Foo>("foo").unwrap();
        assert_eq!(
            foo,
            Some(Foo {
                bar: "xd".into(),
                xd: Some("c".into()),
                nonexistent: None
            })
        );

        let a = node.deserialize::<A>("foo.a").unwrap();
        assert_eq!(a, Some(A { a: 1 }));

        let fooa = node.deserialize::<FooWithA>("foo").unwrap();
        assert_eq!(
            fooa,
            Some(FooWithA {
                bar: "xd".into(),
                xd: Some("c".into()),
                nonexistent: None,
                a: A { a: 1 },
            })
        );

        let err = node.deserialize::<FooWithA>("a").unwrap_err();
        assert_eq!(
            err.to_string(),
            "failed to parse the configuration from the builtin source
invalid type: integer `1`, expected struct FooWithA"
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

        let a = node.deserialize::<IntOrString>("a").unwrap();
        assert_eq!(a, Some(IntOrString::Int(1)));
        let c = node.deserialize::<IntOrString>("c").unwrap();
        assert_eq!(c, Some(IntOrString::String("xd".into())));

        let a = node.deserialize::<Sourced<IntOrString>>("a").unwrap();
        assert_eq!(
            a,
            Some(Sourced {
                value: IntOrString::Int(1),
                source: SOURCE
            })
        );

        let err = node.deserialize::<IntOrString>("foo").unwrap_err();
        assert_eq!(
            err.to_string(),
            "failed to parse the configuration from the builtin source
invalid type: map, expected an int or a string"
        );

        let value = node.deserialize::<IntOrString>("nonexistentkey").unwrap();
        assert_eq!(value, None);
    }
}
