use std::fmt;

#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub struct SetOnce {
    was_set: bool,
}

impl SetOnce {
    pub fn new() -> Self {
        Self { was_set: false }
    }

    pub fn set(&mut self) {
        self.was_set = true;
    }

    pub fn was_set(&self) -> bool {
        self.was_set
    }
}

impl Default for SetOnce {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Display for SetOnce {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = if self.was_set() {
            "was set"
        } else {
            "was not set"
        };
        write!(f, "{}", text)
    }
}
