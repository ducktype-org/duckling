use serde::Deserialize;

use super::Response;

pub trait ResponseExt {
    fn deserialize_json<T>(&self) -> serde_json::Result<T>
    where
        T: for<'de> Deserialize<'de>;
}

impl ResponseExt for Response {
    fn deserialize_json<T>(&self) -> serde_json::Result<T>
    where
        T: for<'de> Deserialize<'de>,
    {
        serde_json::from_slice(self.body())
    }
}
