use std::{any::Any, fmt};

use crate::QuackResult;

/// Quackpack error type, contains a stack of messages for the user.
pub struct QuackError {
    inner: Vec<QpErrorType>,
    exit_code: i32,
}

/// Enum for single messages on the error stack.
/// The underlying options represent:
///     Error - error at the user side (wrong usage of the program).
///     Internal - program's internal logic error, means a critical bug is present.
///     Hint - a suggestion for the user how to fix the error.
///     Note - any additional information that the user should know.
///
/// # Usage
/// The errors are added on a stack, so when adding an error with a hint, the hint should be added before the error.
///
pub enum QpErrorType {
    Error(Box<dyn AsRef<str>>),
    Internal(Box<dyn AsRef<str>>),
    Hint(Box<dyn AsRef<str>>),
    Note(Box<dyn AsRef<str>>),
}

impl Default for QuackError {
    fn default() -> Self {
        Self::new()
    }
}

impl QuackError {
    /// Creates an empty QuackError.
    pub fn new() -> Self {
        Self {
            inner: Vec::new(),
            exit_code: 1,
        }
    }

    /// Creates a QuackError with a single error message.
    pub fn error<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Error(Box::new(err))],
            exit_code: 1,
        }
    }

    /// Creates a QuackError with a single internal error message.
    pub fn internal<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Internal(Box::new(err))],
            exit_code: 1,
        }
    }

    /// Adds a hint on top of the QuackError's stack.
    pub fn add_hint<T>(mut self, hint: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        self.inner.push(QpErrorType::Hint(Box::new(hint)));
        self
    }

    /// Adds a note on top of the QuackError's stack.
    pub fn add_note<T>(mut self, note: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        self.inner.push(QpErrorType::Note(Box::new(note)));
        self
    }

    /// Adds an error on top of the QuackError's stack.
    pub fn context<T>(mut self, ctx: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        self.inner.push(QpErrorType::Error(Box::new(ctx)));
        self
    }

    /// Adds an internal error on top of the QuackError's stack.
    pub fn context_internal<T>(mut self, ctx: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        self.inner.push(QpErrorType::Internal(Box::new(ctx)));
        self
    }

    /// Sets the exit code of the error.
    pub fn set_exit_code(mut self, new_code: i32) -> Self {
        self.exit_code = new_code;
        self
    }

    /// Gets the exit code of the error.
    pub fn exit_code(&self) -> i32 {
        self.exit_code
    }

    /// Iterates over the messages stack, from the top to the bottom.
    pub fn stack(&self) -> impl Iterator<Item = &QpErrorType> {
        self.inner.iter().rev()
    }
}

impl fmt::Display for QuackError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (i, x) in self.stack().enumerate() {
            if i < self.inner.len() - 1 {
                writeln!(f, "{x}")?;
            } else {
                write!(f, "{x}")?;
            }
        }
        Ok(())
    }
}

impl fmt::Debug for QuackError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <Self as fmt::Display>::fmt(self, f)
    }
}

impl fmt::Display for QpErrorType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            QpErrorType::Error(error) => write!(f, "{}", error.as_ref().as_ref()),
            QpErrorType::Internal(error) => write!(f, "{}", error.as_ref().as_ref()),
            QpErrorType::Hint(hint) => write!(f, "{}", hint.as_ref().as_ref()),
            QpErrorType::Note(note) => write!(f, "{}", note.as_ref().as_ref()),
        }
    }
}

impl fmt::Debug for QpErrorType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        <Self as fmt::Display>::fmt(self, f)
    }
}

/// Returns with a single error message QuackError wrapped in Result::Err.
#[macro_export]
macro_rules! qp_bail {
    ($msg:literal $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            return Err($crate::QuackError::error(static_msg));
        }
        else {
            return Err($crate::QuackError::error(format!($msg)));
        }
    }};
    ($err:expr $(,)?) => {{
        return Err($err.into());
    }};
    ($fmt:expr, $($args:expr),+ $(,)?) => {
        return Err($crate::QuackError::error(format!($fmt, $($args),*)))
    };
}

/// Creates a single error message QuackError.
#[macro_export]
macro_rules! qp_err {
    ($msg:expr $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::error(static_msg)
        }
        else {
            $crate::QuackError::error(format!($msg))
        }
    }};
    ($err:expr $(,)?) => {{
        $err.into()
    }};
    ($fmt:expr, $($args:expr),+ $(,)?) => {
        $crate::QuackError::error(format!($fmt, $($args),*))
    };
}

/// Returns with a single internal error message QuackError wrapped in Result::Err.
#[macro_export]
macro_rules! qp_bail_internal {
    ($msg:expr $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            return Err($crate::QuackError::internal(static_msg));
        }
        else {
            return Err($crate::QuackError::internal(format!($msg)));
        }
    }};
    ($err:expr $(,)?) => {{
        return Err($err.into());
    }};
    ($fmt:expr, $($args:expr),+ $(,)?) => {
        return Err($crate::QuackError::internal(format!($fmt, $($args),*)))
    };
}

/// Creates a single internal error message QuackError.
#[macro_export]
macro_rules! qp_internal {
    ($msg:expr $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::internal(static_msg)
        }
        else {
            $crate::QuackError::internal(format!($msg))
        }
    }};
    ($err:expr $(,)?) => {{
        return Err($err.into());
    }};
    ($fmt:expr, $($args:expr),+ $(,)?) => {
        $crate::QuackError::internal(format!($fmt, $($args),*))
    };
}

/// A trait for adding contexts to results.
pub trait QuackResultContext<T, E> {
    /// Transforms self into a QuackResult<T> and adds context.
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static;

    /// Transforms self into a QuackResult<T> and adds internal error context.
    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static;

    /// Transforms self into a QuackResult<T>, computes and adds context.
    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C;

    /// Transforms self into a QuackResult<T>, computes and adds internal error context.
    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C;
}

impl<T, E> QuackResultContext<T, E> for Result<T, E>
where
    E: Into<QuackError>,
{
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
    {
        self.map_err(|e| e.into().context(ctx))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
    {
        self.map_err(|e| e.into().context_internal(ctx))
    }

    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C,
    {
        self.map_err(|e| e.into().context(ctx()))
    }

    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C,
    {
        self.map_err(|e| e.into().context_internal(ctx()))
    }
}

impl<T> QuackResultContext<T, QuackError> for Option<T> {
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
    {
        self.ok_or_else(|| QuackError::error(ctx))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
    {
        self.ok_or_else(|| QuackError::internal(ctx))
    }

    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C,
    {
        self.ok_or_else(|| QuackError::error(ctx()))
    }

    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: AsRef<str> + Sized + 'static,
        F: FnOnce() -> C,
    {
        self.ok_or_else(|| QuackError::internal(ctx()))
    }
}

impl<E> From<E> for QuackError
where
    E: std::error::Error + 'static,
{
    fn from(value: E) -> Self {
        let test_err = &value as &dyn Any;
        if let Some(clap_err) = test_err.downcast_ref::<clap::Error>() {
            let err = QuackError::error(format!("{}", clap_err.render().ansi()));
            if matches!(clap_err.kind(), clap::error::ErrorKind::DisplayHelp) {
                err.set_exit_code(0)
            } else {
                err
            }
        } else {
            QuackError::error(value.to_string())
        }
    }
}
