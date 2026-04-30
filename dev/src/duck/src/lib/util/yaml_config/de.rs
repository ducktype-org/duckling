//! A [`Deserializer`](serde::Deserializer) support for [`YamlConfig`].
use std::fmt;

use serde::de;

use super::YamlConfig;
use crate::QuackError;

/// A deserializer for [`YamlConfig`].
pub(super) struct YamlDeserializer<'config, 'key> {
    pub(super) config: &'config YamlConfig,
    pub(super) key: &'key str,
}

pub(super) struct QuackErrorWrapper {
    error: QuackError,
}

impl fmt::Debug for QuackErrorWrapper {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        <QuackError as fmt::Debug>::fmt(&self.error, f)
    }
}

impl fmt::Display for QuackErrorWrapper {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        <QuackError as fmt::Display>::fmt(&self.error, f)
    }
}

impl std::error::Error for QuackErrorWrapper {}

impl From<QuackError> for QuackErrorWrapper {
    fn from(value: QuackError) -> Self {
        Self { error: value }
    }
}

impl de::Error for QuackErrorWrapper {
    fn custom<T>(msg: T) -> Self
    where
        T: fmt::Display,
    {
        QuackError::message(msg).into()
    }
}

macro_rules! forward_to_yaml {
    ($($name: ident $(,$param:ident : $ty:ty)*$(;)?)*) => {
        $(
            fn $name<V>(self $(,$param: $ty)*, visitor: V) -> Result<V::Value, Self::Error>
            where
                V: de::Visitor<'de>
            {
                match self.config.get(self.key)? {
                    Some(v) => v.$name($($param,)* visitor).map_err(|error| {
                        let error: QuackError = error.into();
                        error.into()
                    }),
                    None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
                }
            }
        )*
    }
}

impl<'de> de::Deserializer<'de> for YamlDeserializer<'de, '_> {
    type Error = QuackErrorWrapper;

    forward_to_yaml! {
        deserialize_any;
        deserialize_bool;
        deserialize_i8; deserialize_i16; deserialize_i32; deserialize_i64;
        deserialize_u8; deserialize_u16; deserialize_u32; deserialize_u64;
        deserialize_f32; deserialize_f64;
        deserialize_char; deserialize_str; deserialize_string; deserialize_bytes; deserialize_byte_buf;
        deserialize_unit; deserialize_seq; deserialize_map; deserialize_identifier; deserialize_ignored_any;
        deserialize_unit_struct, name: &'static str;
        deserialize_newtype_struct, name: &'static str;
        deserialize_tuple, len: usize;
        deserialize_tuple_struct, name: &'static str, len: usize;
        deserialize_struct, name: &'static str, fields: &'static [&'static str];
        deserialize_enum, name: &'static str, variants: &'static [&'static str]
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => visitor.visit_some(v).map_err(|error| {
                let error: QuackError = error.into();
                error.into()
            }),
            None => visitor.visit_none(),
        }
    }
}
