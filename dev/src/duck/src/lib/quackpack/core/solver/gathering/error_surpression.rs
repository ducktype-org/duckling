use crate::{QuackError, QuackResult, util::error::QuackMessage};

/// Represents a type with a list of surpressed errors, which occured during some computation.
#[derive(Debug)]
pub struct GathererComputation<T>(pub T, pub Vec<QuackError>);
/// Represents a type with a list of surpressed errors or a critical, not surpressed error.
pub type GathererResult<T> = QuackResult<GathererComputation<T>>;

impl<T: Default> GathererComputation<T> {
    pub fn empty() -> Self {
        Self(T::default(), vec![])
    }

    pub fn only_error(e: QuackError) -> Self {
        Self(T::default(), vec![e])
    }
}

impl<T> GathererComputation<T> {
    pub fn only_success(res: T) -> Self {
        Self(res, vec![])
    }

    pub fn context<C: QuackMessage + Sized + 'static>(mut self, ctx: C) -> Self {
        self.1.push(QuackError::error(ctx));
        self
    }

    pub fn dump_errors(self, errors: &mut Vec<QuackError>) -> T {
        errors.extend(self.1);
        self.0
    }
}

impl<T> From<QuackResult<T>> for GathererComputation<Option<T>> {
    fn from(value: QuackResult<T>) -> Self {
        match value {
            Ok(t) => Self(Some(t), vec![]),
            Err(e) => Self::only_error(e),
        }
    }
}

impl<T> GathererComputation<Vec<T>> {
    pub fn extend(&mut self, partial_comp: Self) {
        self.0.extend(partial_comp.0);
        self.1.extend(partial_comp.1);
    }
}
