use std::error::Error;

use crate::{QuackError, QuackResult};

/// Represents a type with a list of suppressed errors, which occurred during some computation.
/// The idea is to use [`GathererResult`], but store internal errors in the top level Err variant and other
/// errors inside [`GathererComputation`] in Ok variant.
#[derive(Debug)]
pub struct GathererComputation<T>(pub T, pub Vec<QuackError>);
/// Represents a type with a list of suppressed errors or a critical, not suppressed error.
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

    pub fn context<C: Error + Send + Sync + 'static>(mut self, ctx: C) -> Self {
        self.1.push(QuackError::new(ctx));
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
