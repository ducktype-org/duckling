// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! # Deserialization of [`Node`].
//!
//! Most functions are taken from [`IntoDeserializer`] impls, are we're just pushing everything
//! through [`serde`]'s machinery.
//! However, there are some special casing.
//!
//! ## Deserializing a [`Sourced`].
//!
//! Firstly, when using [`Node`], you can use [`Sourced`] to get the [`Source`] which deserialized
//! the given value.
//! It probably won't work with other [`Deserializer`]s.
//!
//! How it works: idea is very simple: we manually implement [`Deserialize`] for [`Sourced`] with a
//! special syntax, and [`NodeDeserializer::deserialize_struct`] checks for that. If it recognises
//! that we're about to handle a [`Sourced`], it forwards to [`SourcedMapAccess`]. See its
//! documentation for more details.
//!
//! [`Sourced`] looks like so:
//! ```rust,ignore
//! struct Sourced<T> {
//!     value: T,
//!     source: Source
//! }
//! ```
//! If we derived [`Deserialize`], it would use names we had written: `Sourced`, `value`, and
//! `source`.
//!
//! Instead, we deserialize [`Sourced`] as if it had illegal names, i.e.:
//! ```rust,ignore
//! struct $__private_Sourced<T> {
//!     $__private_value: T,
//!     $__private_source: Source
//! }
//! ```
//! Since this uses illegal syntax, we can be pretty sure that encountering it means that we're
//! deserializing [`Sourced`].
//!
//! This is handled by [`SourcedMapAccess`].
//! It also has special case for deserializing a [`Source`].
//!
//! ## Deserializing a [`Source`]
//!
//! It happens *only* in [`SourcedMapAccess`], when we're in [`SourcedMapAccessState::Source`].
//!
//! Because we want to have full control over [`Source`], we *treat* it as a tuple.
//!
//! See [`Source::as_deserialize_helper`] and [`Source::from_deserialize_helper`] for actual
//! implementation.
//!
//! However, because tuples don't implement [`IntoDeserializer`] (nor [`Deserializer`]) we have to
//! add some more boilerplate on top.
//! It's handled by [`TupleDeserializer`], which makes tuples (in our case, tuples of size 2) behave
//! like slices (though with different elements). Due to [`serde`]'s choices, we also have to
//! implement [`TupleSeqAccess`].

use std::collections::{HashMap, hash_map};
use std::marker::PhantomData;
use std::path::PathBuf;
use std::{fmt, slice};

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

/// Forward a simple deserialize method to a cast on the [`Node`].
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
            Node::Integer(i64, _) => visitor.visit_i64(*i64),
            Node::Boolean(bool, _) => visitor.visit_bool(*bool),
            Node::String(str, _) => visitor.visit_str(str),
            Node::Seq(seq, _) => visitor.visit_seq(NodeSeqAccess::new(seq)),
            Node::Map(map, _) => visitor.visit_map(NodeMapAccess::new(map)),
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
            visitor.visit_map(SourcedMapAccess::new(self.node))
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

    fn deserialize_newtype_struct<V>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        let _ = name;
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_enum<V>(
        self,
        name: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: Visitor<'de>,
    {
        let _ = name;
        let _ = variants;
        match self.node {
            Node::String(str, _) => visitor.visit_enum(str.as_str().into_deserializer()),
            Node::Map(map, _) => {
                let access = NodeMapAccess::new(map);
                let deserializer = de::value::MapAccessDeserializer::new(access);
                visitor.visit_enum(deserializer)
            }
            _ => Err(de::Error::invalid_type(self.node.unexpected(), &"an enum")),
        }
    }

    forward_to_deserialize_any! {
        i128
        u128
        f32 f64 char string
        bytes byte_buf unit unit_struct
        seq tuple tuple_struct
        map identifier ignored_any
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Helper for tracking the state of [`SourcedMapAccess`].
enum SourcedMapAccessState {
    /// We should deserialize field (i.e. [`Node`]) next.
    Field,
    /// We should deserialize [`Source`] next.
    Source,
    /// We're finished.
    Finished,
}

#[derive(Debug)]
/// A deserializer used by [`Sourced::deserialize`].
///
/// Firstly yields the node, then the source.
///
/// Constructed by special-casing in [`NodeDeserializer::deserialize_struct`].
struct SourcedMapAccess<'a> {
    node: &'a Node,
    state: SourcedMapAccessState,
}

impl<'a> SourcedMapAccess<'a> {
    /// Create a new [`SourcedMapAccess`].
    fn new(node: &'a Node) -> Self {
        Self {
            node,
            state: SourcedMapAccessState::Field,
        }
    }
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
/// A slice-like helper for making tuples behave like [`IntoDeserializer`]/[`Deserializer`].
///
/// We “emulate” tuples like slices with yielding different items.
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
        visitor.visit_seq(TupleSeqAccess::new(self.0, self.1))
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
/// A slice-like helper for making tuples behave like [`SeqAccess`].
///
/// Used by [`TupleDeserializer`].
struct TupleSeqAccess<T, U> {
    first: Option<T>,
    second: Option<U>,
}

impl<T, U> TupleSeqAccess<T, U> {
    /// Create a new [`TupleSeqAccess`].
    fn new(first: T, second: U) -> Self {
        Self {
            first: Some(first),
            second: Some(second),
        }
    }
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
    iter: slice::Iter<'a, Node>,
}

impl<'a> NodeSeqAccess<'a> {
    /// Create a new [`NodeSeqAccess`].
    fn new(slice: &'a [Node]) -> Self {
        Self { iter: slice.iter() }
    }
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
    iter: hash_map::Iter<'a, String, Node>,
    value: Option<&'a Node>,
}

impl<'a> NodeMapAccess<'a> {
    /// Create a new [`NodeMapAccess`].
    fn new(map: &'a HashMap<String, Node>) -> Self {
        Self {
            iter: map.iter(),
            value: None,
        }
    }
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
            None => Ok(None),
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: DeserializeSeed<'de>,
    {
        let node = self.value.take().ok_or_else(|| {
            de::Error::custom(qp_internal!("`next_value_seed` without `next_key_seed`"))
        })?;

        seed.deserialize(NodeDeserializer { node })
    }
}

impl Node {
    /// Helper for [`de::Error::invalid_type`].
    fn unexpected(&self) -> de::Unexpected<'_> {
        match self {
            Node::Integer(num, _) => de::Unexpected::Signed(*num),
            Node::Boolean(b, _) => de::Unexpected::Bool(*b),
            Node::String(str, _) => de::Unexpected::Str(str),
            Node::Seq(..) => de::Unexpected::Seq,
            Node::Map(..) => de::Unexpected::Map,
        }
    }
}

// -----------------------------------------------------------------------------
// Deserialize helpers.
// -----------------------------------------------------------------------------

/// A helper type for deserializing [`Source`].
type SourceDeserializeHelper = (String, String);

impl Source {
    /// Get an appropriate [`SourceDeserializeHelper`] for this [`Source`].
    ///
    /// Keep in sync with [`Self::from_deserialize_helper`] (i.e. roundtripping should not result in
    /// an error).
    fn as_deserialize_helper(&self) -> SourceDeserializeHelper {
        match self {
            Self::Builtin => ("builtin".into(), "".into()),
            Self::Path(path_buf) => ("path".into(), path_buf.to_string_lossy().into_owned()),
        }
    }

    /// Try to construct [`Source`] from the [`SourceDeserializeHelper`].
    ///
    /// Keep in sync with [`Self::as_deserialize_helper`] (i.e. roundtripping should not result in
    /// an error).
    fn from_deserialize_helper(helper: SourceDeserializeHelper) -> QuackResult<Self> {
        match helper.0.as_str() {
            "builtin" => Ok(Self::Builtin),
            "path" => Ok(Self::Path(PathBuf::from(helper.1))),
            _ => qp_bail_internal!("invalid type `{}` for `Source::deserialize`", helper.0),
        }
    }
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

#[derive(Debug, Clone, Eq, PartialEq, Hash, Default)]
/// A wrapped value, with a special [`Deserialize`] impl.
///
/// See [the module level documentation](self) for more information.
pub struct Sourced<T> {
    pub value: T,
    pub source: Source,
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
