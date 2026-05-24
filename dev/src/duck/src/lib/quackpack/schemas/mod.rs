//! Serde schemas used to (de-)serialize manifests.
pub mod frontmatter;
pub mod manifest;
pub mod registry;

use std::hash::Hash;
use std::marker::PhantomData;

use serde::ser::SerializeMap;
use serde::{de, ser};

#[derive(Debug, Eq, PartialEq, Hash, Clone, Copy)]
/// A map with exactly one entry.
pub struct OneEntryMap<K, V> {
    /// The key in the map.
    pub key: K,
    /// The key's value.
    pub value: V,
}

struct OneEntryMapVisitor<K, V>(PhantomData<(K, V)>);
impl<'de, K, V> de::Visitor<'de> for OneEntryMapVisitor<K, V>
where
    K: de::Deserialize<'de>,
    V: de::Deserialize<'de>,
{
    type Value = OneEntryMap<K, V>;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a map with exactly one entry")
    }

    fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
    where
        A: de::MapAccess<'de>,
    {
        let len = map.size_hint().unwrap_or(0);
        match (map.next_entry()?, map.next_entry::<K, V>()?) {
            (Some((key, value)), None) => Ok(OneEntryMap { key, value }),
            _ => Err(de::Error::invalid_length(len, &self)),
        }
    }
}

impl<'de, K, V> de::Deserialize<'de> for OneEntryMap<K, V>
where
    K: de::Deserialize<'de>,
    V: de::Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        deserializer.deserialize_map(OneEntryMapVisitor(PhantomData))
    }
}

impl<K, V> ser::Serialize for OneEntryMap<K, V>
where
    K: ser::Serialize,
    V: ser::Serialize,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        let mut map = serializer.serialize_map(Some(1))?;
        map.serialize_entry(&self.key, &self.value)?;
        map.end()
    }
}
