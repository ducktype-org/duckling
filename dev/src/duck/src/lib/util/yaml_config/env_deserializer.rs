use serde::de::{Deserializer, MapAccess, SeqAccess, Visitor};
use serde_yaml_ng::Value;

use crate::qp_err;
use crate::util::env::Env;
use crate::util::yaml_config::de::QuackErrorWrapper;

pub struct EnvDeserializer<'a> {
    value: Option<Value>,
    path: String,
    env: &'a Env,
}

impl<'a> EnvDeserializer<'a> {
    pub fn new(value: Option<Value>, env: &'a Env, path: String) -> Self {
        Self { value, path, env }
    }

    fn current_env_key(&self) -> String {
        let parts = self.path.split(&['.', '-']);
        let parts = std::iter::once("DUCK").chain(parts);
        parts
            .map(|s| s.to_uppercase().replace('-', "_"))
            .collect::<Vec<_>>()
            .join("_")
    }
}

impl<'de, 'a> Deserializer<'de> for EnvDeserializer<'a> {
    type Error = QuackErrorWrapper;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, QuackErrorWrapper>
    where
        V: Visitor<'de>,
    {
        let env_key = self.current_env_key();

        match self.value {
            Some(Value::Number(n)) => {
                if let Some(env_override) = self.env.get_i64(env_key)? {
                    visitor.visit_i64(env_override)
                } else {
                    visitor.visit_i64(n.as_i64().expect("todo"))
                }
            }
            Some(Value::String(s)) => {
                if let Some(env_override) = self.env.get_str(env_key)? {
                    visitor.visit_string(env_override.to_string())
                } else {
                    visitor.visit_string(s)
                }
            }
            Some(Value::Bool(b)) => {
                if let Some(env_override) = self.env.get_bool(env_key)? {
                    visitor.visit_bool(env_override)
                } else {
                    visitor.visit_bool(b)
                }
            }
            Some(Value::Mapping(map)) => {
                let access = MapDeserializer {
                    iter: map.into_iter(),
                    parent_path: self.path,
                    current_key: None,
                    env: self.env,
                };
                visitor.visit_map(access)
            }
            Some(Value::Sequence(list)) => {
                let access = SeqDeserializer {
                    iter: list.into_iter(),
                    parent_path: self.path,
                    counter: 0,
                    env: self.env,
                };
                visitor.visit_seq(access)
            }
            Some(Value::Null) => visitor.visit_none(),
            Some(Value::Tagged(_)) => panic!("unsupported"),
            None => visitor.visit_none(),
        }
    }

    serde::forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }
}

struct MapDeserializer<'a> {
    iter: <serde_yaml_ng::Mapping as IntoIterator>::IntoIter,
    parent_path: String,
    current_key: Option<(String, Value)>,
    env: &'a Env,
}

impl<'de, 'a> MapAccess<'de> for MapDeserializer<'a> {
    type Error = QuackErrorWrapper;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: serde::de::DeserializeSeed<'de>,
    {
        if let Some((k, v)) = self.iter.next() {
            let Value::String(key_str) = k else {
                return Err(qp_err!("todo").into());
            };
            self.current_key = Some((key_str.clone(), v));
            seed.deserialize(serde::de::value::StringDeserializer::new(key_str))
                .map(Some)
        } else {
            Ok(None)
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: serde::de::DeserializeSeed<'de>,
    {
        let (key, value) = self
            .current_key
            .take()
            .expect("next_value called before next_key");

        let mut new_path = self.parent_path.clone();
        new_path.push('.');
        new_path.push_str(&key);

        let deserializer = EnvDeserializer {
            value: Some(value),
            path: new_path,
            env: self.env,
        };
        seed.deserialize(deserializer)
    }
}

// --- 5. SeqAccess Implementation ---

struct SeqDeserializer<'a> {
    iter: std::vec::IntoIter<Value>,
    parent_path: String,
    env: &'a Env,
    counter: u32,
}

impl<'de, 'a> SeqAccess<'de> for SeqDeserializer<'a> {
    type Error = QuackErrorWrapper;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: serde::de::DeserializeSeed<'de>,
    {
        if let Some(value) = self.iter.next() {
            let mut new_path = self.parent_path.clone();
            new_path.push('.');
            new_path.push_str(&self.counter.to_string());
            self.counter += 1;
            let deserializer = EnvDeserializer {
                value: Some(value),
                path: new_path,
                env: self.env,
            };
            seed.deserialize(deserializer).map(Some)
        } else {
            Ok(None)
        }
    }
}
