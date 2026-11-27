// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Deserialization of [`Node`].

use serde::de::value::Error;
use serde::de::{self, DeserializeSeed, IntoDeserializer, MapAccess, SeqAccess, Visitor};
use serde::{Deserializer, forward_to_deserialize_any};

use super::{Node, SOURCE_FIELD, SOURCED_FIELDS, SOURCED_NAME, Source, VALUE_FIELD};

#[derive(Debug)]
pub struct NodeDeserializer<'a> {
    pub node: &'a Node,
}

impl<'de> Deserializer<'de> for NodeDeserializer<'_> {
    type Error = de::value::Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
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
                state: 0,
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

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128
        u8 u16 u32 u64 u128
        f32 f64 char str string
        bytes byte_buf unit unit_struct
        newtype_struct seq tuple tuple_struct
        map enum identifier ignored_any
    }
}

struct SourcedMapAccess<'a> {
    node: &'a Node,
    state: u8,
}

impl<'de, 'a> MapAccess<'de> for SourcedMapAccess<'a> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: DeserializeSeed<'de>,
    {
        let key = match self.state {
            0 => VALUE_FIELD,
            1 => SOURCE_FIELD,
            _ => return Ok(None),
        };

        seed.deserialize(key.into_deserializer()).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: DeserializeSeed<'de>,
    {
        match self.state {
            0 => {
                self.state = 1;
                seed.deserialize(NodeDeserializer { node: self.node })
            }

            1 => {
                self.state = 2;

                match self.node.source() {
                    Source::Builtin => seed.deserialize(TupleDeserializer(0u32, String::new())),

                    Source::Path(path) => seed
                        .deserialize(TupleDeserializer(1u32, path.to_string_lossy().into_owned())),
                }
            }

            _ => Err(de::Error::custom("unsupported discriminant")),
        }
    }
}

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

struct NodeSeqAccess<'a> {
    iter: std::slice::Iter<'a, Node>,
}

impl<'de, 'a> SeqAccess<'de> for NodeSeqAccess<'a> {
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

struct NodeMapAccess<'a> {
    iter: std::collections::hash_map::Iter<'a, String, Node>,
    value: Option<&'a Node>,
}

impl<'de, 'a> MapAccess<'de> for NodeMapAccess<'a> {
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
        let node = self
            .value
            .take()
            .ok_or_else(|| de::Error::custom("key-value mismatch"))?;

        seed.deserialize(NodeDeserializer { node })
    }
}
