use std::{any::Any, fmt};

use crate::QuackResult;

pub enum DisplayPlace {
    StdOut,
    StdErr,
}

/// Quackpack error type, contains a stack of messages for the user.
pub struct QuackError {
    inner: Vec<QpErrorType>,
    display_place: DisplayPlace,
}

/// Enum for single messages on the error stack.
/// The underlying options represent:
///     Error - error at the user side (wrong usage of the program).
///     Internal - program's internal logic error, means a critical bug is present.
///     Hint - a suggestion for the user how to fix the error.
///     Note - any additional information that the user should know.
///     BareMessage - a non-error message, which does not classify as hint nor note.
///
/// # Usage
/// The errors are added on a stack, so when adding an error with a hint, the hint should be added before the error.
///
pub enum QpErrorType {
    Error(Box<dyn AsRef<str>>),
    Internal(Box<dyn AsRef<str>>),
    Hint(Box<dyn AsRef<str>>),
    Note(Box<dyn AsRef<str>>),
    BareMessage(Box<dyn AsRef<str>>),
}

impl QuackError {
    /// Creates a QuackError with a single error message.
    pub fn error<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Error(Box::new(err))],
            display_place: DisplayPlace::StdErr,
        }
    }

    /// Creates a QuackError with a single internal error message.
    pub fn internal<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Internal(Box::new(err))],
            display_place: DisplayPlace::StdErr,
        }
    }

    /// Creates a QuackError with a single hint message.
    pub fn hint<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Hint(Box::new(err))],
            display_place: DisplayPlace::StdOut,
        }
    }

    /// Creates a QuackError with a single note message.
    pub fn note<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::Note(Box::new(err))],
            display_place: DisplayPlace::StdOut,
        }
    }

    /// Creates a QuackError with a single bare message.
    pub fn bare_message<T>(err: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        Self {
            inner: vec![QpErrorType::BareMessage(Box::new(err))],
            display_place: DisplayPlace::StdOut,
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

    /// Adds a bare message on top of the QuackError's stack.
    pub fn add_bare_message<T>(mut self, note: T) -> Self
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
        self.set_display_place(DisplayPlace::StdErr);
        self
    }

    /// Adds an internal error on top of the QuackError's stack.
    pub fn context_internal<T>(mut self, ctx: T) -> Self
    where
        T: AsRef<str> + Sized + 'static,
    {
        self.inner.push(QpErrorType::Internal(Box::new(ctx)));
        self.set_display_place(DisplayPlace::StdErr);
        self
    }

    /// Gets the exit code of the error.
    pub fn exit_code(&self) -> i32 {
        match self.display_place {
            DisplayPlace::StdOut => 0,
            DisplayPlace::StdErr => 1,
        }
    }

    /// Sets the display place.
    pub fn set_display_place(&mut self, display_place: DisplayPlace) {
        self.display_place = display_place;
    }

    /// Gets the display place.
    pub fn display_place(&self) -> &DisplayPlace {
        &self.display_place
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
            QpErrorType::BareMessage(msg) => write!(f, "{}", msg.as_ref().as_ref()),
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
        return Err($crate::qp_err!($msg))
    }};
    ($err:expr $(,)?) => {{
        return Err($crate::qp_err!($err))
    }};
    ($fmt:expr, $($args:tt)*) => {
        return Err($crate::qp_err!($fmt, $($args)*))
    };
}

/// Creates a single error message QuackError.
#[macro_export]
macro_rules! qp_err {
    ($msg:literal $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::error(static_msg)
        } else {
            $crate::QuackError::error(format!($msg))
        }
    }};
    ($err:expr $(,)?) => {{
        $err.into()
    }};
    ($fmt:expr, $($args:tt)*) => {
        $crate::QuackError::error(format!($fmt, $($args)*))
    };
}

/// Returns with a single internal error message QuackError wrapped in Result::Err.
#[macro_export]
macro_rules! qp_bail_internal {
    ($msg:literal $(,)?) => {{
        return Err($crate::qp_internal!($msg))
    }};
    ($err:expr $(,)?) => {{
        return Err($crate::qp_internal!($err))
    }};
    ($fmt:expr, $($args:tt)*) => {
        return Err($crate::qp_internal!($fmt, $($args)*))
    };
}

/// Creates a single internal error message QuackError.
#[macro_export]
macro_rules! qp_internal {
    ($msg:literal $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::internal(static_msg)
        } else {
            $crate::QuackError::internal(format!($msg))
        }
    }};
    ($err:expr $(,)?) => {{
        return Err($err.into());
    }};
    ($fmt:expr, $($args:tt)*) => {
        $crate::QuackError::internal(format!($fmt, $($args)*))
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
            if clap_err.use_stderr() {
                let mut err = QuackError::error(format!("{}", clap_err.render().ansi()));
                err.set_display_place(DisplayPlace::StdErr);
                err
            } else {
                let mut err = QuackError::bare_message(format!("{}", clap_err.render().ansi()));
                err.set_display_place(DisplayPlace::StdOut);
                err
            }
        } else {
            QuackError::error(value.to_string())
        }
    }
}
