use std::{fmt, str::FromStr};

use anyhow::bail;
use serde::{de, ser};

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd, Hash, Clone, Copy)]
pub struct Version {
    major: u64,
    minor: u64,
    patch: u64,
}

impl Version {
    pub const fn new(major: u64, minor: u64, patch: u64) -> Self {
        Self {
            major,
            minor,
            patch,
        }
    }

    pub const fn major(&self) -> u64 {
        self.major
    }

    pub const fn minor(&self) -> u64 {
        self.minor
    }

    pub const fn patch(&self) -> u64 {
        self.patch
    }

    pub fn can_be_upgraded_to(&self, other: &Version) -> bool {
        if self.major != other.major {
            return false;
        }
        if self.major == 0 {
            return self.minor == other.minor && self.patch <= other.patch;
        }
        self <= other
    }

    pub fn to_string_without_trailing_zeros(&self) -> String {
        if self.patch == 0 && self.minor == 0 {
            format!("{}", self.major)
        } else if self.patch == 0 {
            format!("{}.{}", self.major, self.minor)
        } else {
            format!("{}.{}.{}", self.major, self.minor, self.patch)
        }
    }

    pub fn format_without_trailing_zeros(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.patch == 0 && self.minor == 0 {
            write!(f, "{}", self.major)
        } else if self.patch == 0 {
            write!(f, "{}.{}", self.major, self.minor)
        } else {
            write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
        }
    }

    pub const fn bump_patch(&self) -> Self {
        let mut copy = *self;
        copy.patch += 1;
        copy
    }

    pub fn bump_minor(&self) -> Self {
        Self::from((self.major, self.minor + 1))
    }

    pub fn bump_major(&self) -> Self {
        Self::from(self.major + 1)
    }
}

impl Default for Version {
    fn default() -> Self {
        Self::new(0, 1, 0)
    }
}

impl From<(u64, u64, u64)> for Version {
    fn from(value: (u64, u64, u64)) -> Self {
        Self::new(value.0, value.1, value.2)
    }
}

impl From<(u64, u64)> for Version {
    fn from(value: (u64, u64)) -> Self {
        Self::new(value.0, value.1, 0)
    }
}

impl From<u64> for Version {
    fn from(value: u64) -> Self {
        Self::new(value, 0, 0)
    }
}

impl fmt::Display for Version {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

impl ser::Serialize for Version {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl FromStr for Version {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut splitted = s.split('.');
        match (
            splitted.next(),
            splitted.next(),
            splitted.next(),
            splitted.next(),
        ) {
            (Some(major), None, None, None) => Ok(Self::new(major.parse()?, 0, 0)),
            (Some(major), Some(minor), None, None) => {
                Ok(Self::new(major.parse()?, minor.parse()?, 0))
            }
            (Some(major), Some(minor), Some(patch), None) => {
                Ok(Self::new(major.parse()?, minor.parse()?, patch.parse()?))
            }
            _ => bail!("expected a version in the format `X`, `X.Y`, or `X.Y.Z`"),
        }
    }
}

impl<'de> de::Deserialize<'de> for Version {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        let as_str = <&'de str>::deserialize(deserializer)?;
        Version::from_str(as_str).map_err(de::Error::custom)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deserialize() {
        assert_eq!("1".parse::<Version>().unwrap(), Version::new(1, 0, 0));
        assert_eq!("1.0".parse::<Version>().unwrap(), Version::new(1, 0, 0));
        assert_eq!("1.1".parse::<Version>().unwrap(), Version::new(1, 1, 0));
        assert_eq!(
            "1.12.24".parse::<Version>().unwrap(),
            Version::new(1, 12, 24)
        );

        assert_eq!(
            "1.12.24.".parse::<Version>().unwrap_err().to_string(),
            "expected a version in the format `X`, `X.Y`, or `X.Y.Z`"
        );
        assert_eq!(
            "1.12.24.1".parse::<Version>().unwrap_err().to_string(),
            "expected a version in the format `X`, `X.Y`, or `X.Y.Z`"
        );
        assert_eq!(
            "a".parse::<Version>().unwrap_err().to_string(),
            "invalid digit found in string"
        );
        assert_eq!(
            "1.a.24.".parse::<Version>().unwrap_err().to_string(),
            "expected a version in the format `X`, `X.Y`, or `X.Y.Z`"
        );
        assert_eq!(
            "1.12.b.".parse::<Version>().unwrap_err().to_string(),
            "expected a version in the format `X`, `X.Y`, or `X.Y.Z`"
        );
        assert_eq!(
            "1.12..".parse::<Version>().unwrap_err().to_string(),
            "expected a version in the format `X`, `X.Y`, or `X.Y.Z`"
        );
        assert_eq!(
            "-1".parse::<Version>().unwrap_err().to_string(),
            "invalid digit found in string"
        );
        assert_eq!(
            "1.-12".parse::<Version>().unwrap_err().to_string(),
            "invalid digit found in string"
        );
        assert_eq!(
            "1.".parse::<Version>().unwrap_err().to_string(),
            "cannot parse integer from empty string"
        );
    }

    #[test]
    fn compare() {
        let v = Version::from_str("1.2.3").unwrap();
        let x = Version::new(1, 2, 3);
        assert_eq!(v, x);
        let x = Version::new(1, 2, 0);
        assert_ne!(v, x);
        assert!(x < v);
        assert!(v > x);
        assert!(!v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&v));
        let x = Version::new(21, 3, 7);
        assert!(v < x);
    }

    #[test]
    fn update_to_positive_major() {
        let v = Version::new(1, 2, 3);
        let x = Version::from(2);
        assert!(!x.can_be_upgraded_to(&v));
        assert!(!v.can_be_upgraded_to(&x));
        assert!(v.can_be_upgraded_to(&v));
        assert!(x.can_be_upgraded_to(&x));

        let x = Version::new(1, 2, 2);
        assert!(x.can_be_upgraded_to(&v));
        assert!(!v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&x));

        let x = Version::new(1, 2, 4);
        assert!(!x.can_be_upgraded_to(&v));
        assert!(v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&x));

        let x = v.bump_minor();
        assert!(!x.can_be_upgraded_to(&v));
        assert!(v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&x));
    }

    #[test]
    fn update_to_zero_major() {
        let v = Version::new(0, 3, 7);
        let x = Version::from((0, 4));
        assert!(!x.can_be_upgraded_to(&v));
        assert!(!v.can_be_upgraded_to(&x));
        assert!(v.can_be_upgraded_to(&v));
        assert!(x.can_be_upgraded_to(&x));

        let x = Version::new(0, 3, 6);
        assert!(x.can_be_upgraded_to(&v));
        assert!(!v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&x));

        let x = Version::new(0, 2, 0);
        assert!(!x.can_be_upgraded_to(&v));
        assert!(!v.can_be_upgraded_to(&x));
        assert!(x.can_be_upgraded_to(&x));
    }

    #[test]
    fn bump() {
        let v = Version::new(1, 2, 3);
        assert_eq!(v.bump_patch(), Version::new(1, 2, 4));
        assert_eq!(v.bump_minor(), Version::new(1, 3, 0));
        assert_eq!(v.bump_major(), Version::from(2));
        assert_eq!(Version::new(0, 1, 0).bump_major(), Version::from(1));
        assert_eq!(Version::from((0, 1)).bump_patch(), Version::new(0, 1, 1));
    }

    #[test]
    fn as_string() {
        assert_eq!(Version::new(1, 0, 0).to_string(), "1.0.0");
        assert_eq!(
            Version::new(1, 0, 0).to_string_without_trailing_zeros(),
            "1"
        );

        assert_eq!(Version::new(0, 1, 0).to_string(), "0.1.0");
        assert_eq!(
            Version::new(0, 1, 0).to_string_without_trailing_zeros(),
            "0.1"
        );

        assert_eq!(Version::new(0, 0, 0).to_string(), "0.0.0");
        assert_eq!(
            Version::new(0, 0, 0).to_string_without_trailing_zeros(),
            "0"
        );

        assert_eq!(Version::new(1, 2, 3).to_string(), "1.2.3");
        assert_eq!(
            Version::new(1, 2, 3).to_string_without_trailing_zeros(),
            "1.2.3"
        );

        assert_eq!(Version::new(1, 0, 10).to_string(), "1.0.10");
        assert_eq!(
            Version::new(1, 0, 10).to_string_without_trailing_zeros(),
            "1.0.10"
        );
    }
}
