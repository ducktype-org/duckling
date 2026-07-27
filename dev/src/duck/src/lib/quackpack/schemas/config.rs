//! Schemas used by the user configuration file.

use std::ops::{Deref, DerefMut};
use std::str::FromStr;
use std::time::Duration;

use serde::{Deserialize, de};
use url::Url;

use crate::{QuackError, QuackResultContext, qp_bail};

#[derive(Debug, Clone, Deserialize, Default)]
/// A struct for the `security:` config map.
#[serde(rename_all = "kebab-case")]
pub struct SecurityConfig {
    pub typos: Option<SecurityTyposConfig>,
}

#[derive(Debug, Clone, Deserialize, Default)]
/// A struct for the `security: typos:` config map.
#[serde(rename_all = "kebab-case")]
pub struct SecurityTyposConfig {
    /// Radius of Levenshtein distance in which we try to fix typos.
    pub max_distance: Option<u64>,
    /// Whether fixing typos if allowed. `None` has the same behaviour as `Some(false)`.
    pub enabled: Option<bool>,
}

#[derive(Debug, Clone, Deserialize, Default)]
#[serde(rename_all = "kebab-case")]
/// A struct for the `registry:` config map.
pub struct RegistryConfig {
    /// If specified, this overrides the default registry URL.
    pub url: Option<Url>,
}

#[derive(Debug, Clone, Deserialize, Default)]
#[serde(rename_all = "kebab-case")]
/// A struct for the `storage:` config map.
pub struct StorageConfig {
    /// Maximal allowed lifetime of temporary venvs.
    pub temporary_lifetime: Option<HumanDeserializableDuration>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
/// Duration deserialized from format `<count> <unit>`.
/// Allowed units are: "second", "minute", "hour", "day", "week", and their plural forms.
pub struct HumanDeserializableDuration(pub Duration);

impl Deref for HumanDeserializableDuration {
    type Target = Duration;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for HumanDeserializableDuration {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl FromStr for HumanDeserializableDuration {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let Some((count, unit)) = s.split_once(' ') else {
            qp_bail!("expected a duration in format `<count> <unit>`")
        };
        let count = count.parse::<u64>().context("<count> wasn't a u64")?;
        // !TODO: Use `from_<unit>(count)`, after bumping rust's version in CI to 1.91.0.
        let seconds_in_unit = match unit {
            "second" | "seconds" => 1,
            "minute" | "minutes" => 60,
            "hour" | "hours" => 60 * 60,
            "day" | "days" => 24 * 60 * 60,
            "week" | "weeks" => 7 * 24 * 60 * 60,
            _ => {
                return Err(QuackError::note(
                    "known formats are: second, minute, hour, day, week, and their plural forms",
                ))
                .context(format!("unknown time format `{unit}`"));
            }
        };
        let duration = Duration::from_secs(count * seconds_in_unit);
        Ok(Self(duration))
    }
}

impl<'de> Deserialize<'de> for HumanDeserializableDuration {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a duration in format `<count> <unit>`")
            .string(|s| s.parse().map_err(de::Error::custom))
            .deserialize(deserializer)
    }
}

#[cfg(test)]
mod tests {
    use std::time::Duration;

    use super::HumanDeserializableDuration;
    use crate::QuackResult;

    fn parse(s: &str) -> QuackResult<HumanDeserializableDuration> {
        s.parse()
    }

    fn err(s: &str) -> String {
        parse(s).unwrap_err().to_string()
    }

    fn duration(s: &str) -> Duration {
        parse(s).unwrap().0
    }

    #[test]
    fn human_duration_fails() {
        assert_eq!(err(""), "expected a duration in format `<count> <unit>`");
        assert_eq!(
            err("-1 u"),
            "<count> wasn't a u64
invalid digit found in string"
        );

        assert_eq!(
            err("1 u"),
            "unknown time format `u`
known formats are: second, minute, hour, day, week, and their plural forms"
        );
    }

    #[test]
    fn human_duration() {
        // Seconds
        assert_eq!(duration("0 seconds"), Duration::default());
        assert_eq!(duration("1 second"), Duration::from_secs(1));
        assert_eq!(duration("2 seconds"), Duration::from_secs(2));

        // Minutes
        assert_eq!(duration("1 minute"), Duration::from_secs(60));
        assert_eq!(duration("2 minutes"), Duration::from_secs(120));

        // Hours
        assert_eq!(duration("1 hour"), Duration::from_secs(3600));
        assert_eq!(duration("2 hours"), Duration::from_secs(7200));

        // Days
        assert_eq!(duration("1 day"), Duration::from_secs(86_400));
        assert_eq!(duration("2 days"), Duration::from_secs(172_800));

        // Weeks
        assert_eq!(duration("1 week"), Duration::from_secs(604_800));
        assert_eq!(duration("2 weeks"), Duration::from_secs(1_209_600));
    }
}
