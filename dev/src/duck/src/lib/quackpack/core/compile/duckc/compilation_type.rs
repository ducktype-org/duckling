use std::fmt;

#[non_exhaustive]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
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
