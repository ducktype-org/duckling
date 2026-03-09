//! High level type of compilation, supported by both duck and duckc.
//!
//! Right now we only support compiling the root package, but in future there'll be more options.
use std::fmt;

#[non_exhaustive]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
/// High level type of compilation, supported by both duck and duckc.
///
/// Right now we only support compiling the root package, but in future there'll be more options.
pub enum CompilationType {
    OnlyRootPackage,
}

impl fmt::Display for CompilationType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::OnlyRootPackage => write!(f, "only root package"),
        }
    }
}
