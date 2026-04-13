//! A [`Deserializer`](serde::Deserializer) support for [`YamlConfig`].
use std::fmt;

use serde::de;

use super::YamlConfig;
use crate::QuackError;
use crate::util::error::MessageError;

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
        QuackError::new(MessageError(msg.to_string().into())).into()
    }
}

macro_rules! forward_to_yaml {
    ( $($name:ident)*) => {
        $(
            fn $name<V>(self, visitor: V) -> Result<V::Value, Self::Error>
            where
                V: de::Visitor<'de>
            {
                match self.config.get(self.key)? {
                    Some(v) => v.clone().$name(visitor).map_err(|error| {
                        let error: QuackError = error.into();
                        error.into()
                    }),
                    None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
                }
            }
        )*
    }
}

impl<'de> de::Deserializer<'de> for YamlDeserializer<'_, '_> {
    type Error = QuackErrorWrapper;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v.clone().deserialize_any(visitor).map_err(|error| {
                let error: QuackError = error.into();
                error.into()
            }),
            None => visitor.visit_none(),
        }
    }

    forward_to_yaml! {
        deserialize_bool
            deserialize_i8 deserialize_i16 deserialize_i32 deserialize_i64
            deserialize_u8 deserialize_u16 deserialize_u32 deserialize_u64
            deserialize_f32 deserialize_f64
            deserialize_char deserialize_str deserialize_string deserialize_bytes deserialize_byte_buf
            deserialize_unit deserialize_seq deserialize_map deserialize_identifier deserialize_ignored_any
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => visitor.visit_some(v.clone()).map_err(|error| {
                let error: QuackError = error.into();
                error.into()
            }),
            None => visitor.visit_none(),
        }
    }

    fn deserialize_unit_struct<V>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v
                .clone()
                .deserialize_unit_struct(name, visitor)
                .map_err(|error| {
                    let error: QuackError = error.into();
                    error.into()
                }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }

    fn deserialize_newtype_struct<V>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v
                .clone()
                .deserialize_newtype_struct(name, visitor)
                .map_err(|error| {
                    let error: QuackError = error.into();
                    error.into()
                }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }

    fn deserialize_tuple<V>(self, len: usize, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v.clone().deserialize_tuple(len, visitor).map_err(|error| {
                let error: QuackError = error.into();
                error.into()
            }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }

    fn deserialize_tuple_struct<V>(
        self,
        name: &'static str,
        len: usize,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v
                .clone()
                .deserialize_tuple_struct(name, len, visitor)
                .map_err(|error| {
                    let error: QuackError = error.into();
                    error.into()
                }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }

    fn deserialize_struct<V>(
        self,
        name: &'static str,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v
                .clone()
                .deserialize_struct(name, fields, visitor)
                .map_err(|error| {
                    let error: QuackError = error.into();
                    error.into()
                }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }

    fn deserialize_enum<V>(
        self,
        name: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match self.config.get(self.key)? {
            Some(v) => v
                .clone()
                .deserialize_enum(name, variants, visitor)
                .map_err(|error| {
                    let error: QuackError = error.into();
                    error.into()
                }),
            None => Err(de::Error::custom(format!("missing key `{}`", self.key))),
        }
    }
}
