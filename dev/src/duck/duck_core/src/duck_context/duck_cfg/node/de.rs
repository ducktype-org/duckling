// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Deserialization of [`Node`].

use std::fmt;
use std::marker::PhantomData;
use std::path::PathBuf;

use serde::de::value::Error;
use serde::de::{self, DeserializeSeed, IntoDeserializer, MapAccess, SeqAccess, Visitor};
use serde::{Deserialize, Deserializer, forward_to_deserialize_any};

use super::{Node, Source};
use crate::{QuackResult, qp_bail_internal, qp_internal};

// -----------------------------------------------------------------------------
// Deserializers
// -----------------------------------------------------------------------------

const SOURCED_NAME: &str = "$__private_Sourced";
const VALUE_FIELD: &str = "$__private_value";
const SOURCE_FIELD: &str = "$__private_source";
const SOURCED_FIELDS: &[&str] = &[VALUE_FIELD, SOURCE_FIELD];

#[derive(Debug)]
pub struct NodeDeserializer<'a> {
    pub node: &'a Node,
}

macro_rules! forward_to_node {
    ($($serde:ident $visit:ident $cast:ident)*) => {
        $(
            fn $serde<V>(self, visitor: V) -> Result<V::Value, Self::Error>
            where
                V: Visitor<'de>,
            {
                // @TODO: #4243 Add source.
                let value = self.node.$cast().map_err(de::Error::custom)?;
                visitor.$visit(value)
            }
        )*
    };
}

impl<'de> Deserializer<'de> for NodeDeserializer<'de> {
    type Error = de::value::Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        // @TODO: #4243 Add source.
        match self.node {
            Node::Integer(value, _) => visitor.visit_i64(*value),
            Node::Boolean(value, _) => visitor.visit_bool(*value),
            Node::String(value, _) => visitor.visit_str(value),
            Node::Seq(values, _) => visitor.visit_seq(NodeSeqAccess {
                iter: values.iter(),
            }),
            Node::Map(values, _) => visitor.visit_map(NodeMapAccess {
                iter: values.iter(),
                value: None,
            }),
        }
    }

    fn deserialize_struct<V>(
        self,
        name: &'static str,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        if name == SOURCED_NAME && fields == SOURCED_FIELDS {
            visitor.visit_map(SourcedMapAccess {
                node: self.node,
                state: SourcedMapAccessState::Field,
            })
        } else {
            self.deserialize_any(visitor)
        }
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        visitor.visit_some(self)
    }

    forward_to_node! {
        deserialize_bool visit_bool as_bool
        deserialize_i8 visit_i64 as_i64
        deserialize_i16 visit_i64 as_i64
        deserialize_i32 visit_i64 as_i64
        deserialize_i64 visit_i64 as_i64
        deserialize_u8 visit_i64 as_i64
        deserialize_u16 visit_i64 as_i64
        deserialize_u32 visit_i64 as_i64
        deserialize_u64 visit_i64 as_i64
        deserialize_str visit_borrowed_str as_str
    }

    forward_to_deserialize_any! {
        i128
        u128
        f32 f64 char string
        bytes byte_buf unit unit_struct
        newtype_struct seq tuple tuple_struct
        map enum identifier ignored_any
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
enum SourcedMapAccessState {
    Field,
    Source,
    Finished,
}

#[derive(Debug)]
struct SourcedMapAccess<'a> {
    node: &'a Node,
    state: SourcedMapAccessState,
}

impl<'de> MapAccess<'de> for SourcedMapAccess<'de> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: DeserializeSeed<'de>,
    {
        let key = match self.state {
            SourcedMapAccessState::Field => VALUE_FIELD,
            SourcedMapAccessState::Source => SOURCE_FIELD,
            SourcedMapAccessState::Finished => return Ok(None),
        };

        seed.deserialize(key.into_deserializer()).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: DeserializeSeed<'de>,
    {
        match self.state {
            SourcedMapAccessState::Field => {
                self.state = SourcedMapAccessState::Source;
                seed.deserialize(NodeDeserializer { node: self.node })
            }
            SourcedMapAccessState::Source => {
                self.state = SourcedMapAccessState::Finished;
                let helper = self.node.source().as_deserialize_helper();
                seed.deserialize(TupleDeserializer(helper.0, helper.1))
            }
            SourcedMapAccessState::Finished => Err(de::Error::custom(qp_internal!(
                "asked for more than 2 values for `Sourced`"
            ))),
        }
    }
}

#[derive(Debug)]
struct TupleDeserializer<T, U>(T, U);

impl<'de, T, U> Deserializer<'de> for TupleDeserializer<T, U>
where
    T: IntoDeserializer<'de, Error>,
    U: IntoDeserializer<'de, Error>,
{
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        visitor.visit_seq(TupleSeqAccess {
            first: Some(self.0),
            second: Some(self.1),
        })
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128
        u8 u16 u32 u64 u128
        f32 f64 char str string
        bytes byte_buf option unit unit_struct
        newtype_struct seq tuple tuple_struct
        map struct enum identifier ignored_any
    }
}

#[derive(Debug)]
struct TupleSeqAccess<T, U> {
    first: Option<T>,
    second: Option<U>,
}

impl<'de, T, U> SeqAccess<'de> for TupleSeqAccess<T, U>
where
    T: IntoDeserializer<'de, Error>,
    U: IntoDeserializer<'de, Error>,
{
    type Error = Error;

    fn next_element_seed<V>(&mut self, seed: V) -> Result<Option<V::Value>, Self::Error>
    where
        V: DeserializeSeed<'de>,
    {
        if let Some(first) = self.first.take() {
            return seed.deserialize(first.into_deserializer()).map(Some);
        }

        if let Some(second) = self.second.take() {
            return seed.deserialize(second.into_deserializer()).map(Some);
        }

        Ok(None)
    }
}

#[derive(Debug)]
struct NodeSeqAccess<'a> {
    iter: std::slice::Iter<'a, Node>,
}

impl<'de> SeqAccess<'de> for NodeSeqAccess<'de> {
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some(node) => seed.deserialize(NodeDeserializer { node }).map(Some),
            None => Ok(None),
        }
    }
}

#[derive(Debug)]
struct NodeMapAccess<'a> {
    iter: std::collections::hash_map::Iter<'a, String, Node>,
    value: Option<&'a Node>,
}

impl<'de> MapAccess<'de> for NodeMapAccess<'de> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: DeserializeSeed<'de>,
    {
        match self.iter.next() {
            Some((key, value)) => {
                self.value = Some(value);
                seed.deserialize(key.as_str().into_deserializer()).map(Some)
            }
            None => {
                self.value = None;
                Ok(None)
            }
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: DeserializeSeed<'de>,
    {
        let node = self.value.take().ok_or_else(|| {
            de::Error::custom(qp_internal!("key-value mismatch in `NodeMapAccess`"))
        })?;

        seed.deserialize(NodeDeserializer { node })
    }
}

// -----------------------------------------------------------------------------
// Deserialize helpers.
// -----------------------------------------------------------------------------

/// A helper type for deserializing [`Source`].
type SourceDeserializeHelper = (String, String);

impl Source {
    /// Get an appropriate [`SourceDeserializeHelper`] for this [`Source`].
    fn as_deserialize_helper(&self) -> SourceDeserializeHelper {
        match self {
            Self::Builtin => ("builtin".into(), "".into()),
            Self::Path(path_buf) => ("path".into(), path_buf.to_string_lossy().into_owned()),
        }
    }

    /// Try to construct [`Source`] from the [`SourceDeserializeHelper`].
    fn from_deserialize_helper(helper: SourceDeserializeHelper) -> QuackResult<Self> {
        match helper.0.as_str() {
            "builtin" => Ok(Self::Builtin),
            "path" => Ok(Self::Path(PathBuf::from(helper.1))),
            _ => qp_bail_internal!("invalid type `{}` for `Source::deserialize`", helper.0),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Hash, Default)]
/// A wrapped value, with a special [`Deserialize`] impl.
pub struct Sourced<T> {
    pub value: T,
    pub source: Source,
}

impl<'de> Deserialize<'de> for Source {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let helper = SourceDeserializeHelper::deserialize(deserializer)?;
        Source::from_deserialize_helper(helper).map_err(serde::de::Error::custom)
    }
}

impl<'de, T> Deserialize<'de> for Sourced<T>
where
    T: Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct SourcedVisitor<T>(PhantomData<T>);

        impl<'de, T> Visitor<'de> for SourcedVisitor<T>
        where
            T: Deserialize<'de>,
        {
            type Value = Sourced<T>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a value with source")
            }

            fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let value = map.next_value()?;
                let source = map.next_value()?;

                Ok(Sourced { value, source })
            }
        }

        deserializer.deserialize_struct(SOURCED_NAME, SOURCED_FIELDS, SourcedVisitor(PhantomData))
    }
}
