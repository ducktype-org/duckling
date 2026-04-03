//! The home of [`QuackError`] struct and [`QuackResultContext`] trait, which are building blocks
//! of error handling in duck and quackpack.

// NOTE: In this file, we avoid generics as much as possible.
//
// On the first sight, `create_messages_errors!` could produce `$name<M>(pub M)`, and
// `ContextError` could be generic over `<C, E>`, but this leads to some difficulties with
// downcasting and/or is:
// 1. When downcasting, we would have to provide all generic arguments, even if we want to check
//    "is it this type, with any generics",
// 2. especially, this makes `error_type` implementation harder: because of the use of contexts,
//    most of the error stack is `ContextError`, and at the end we have some first error, and
//    when checking `is`, we really want to call downcast/is on the `context` field in the
//    `ContextError` struct, not on the entire `ContextError` struct.
use std::borrow::Cow;
use std::error::Error;
use std::fmt;

use crate::QuackResult;

type BoxError = Box<dyn Error + Send + Sync + 'static>;

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
/// Where to display an error.
pub enum DisplayPlace {
    StdOut,
    StdErr,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// A type of a single [`Error`] on the errors' stack.
///
/// Used for printing.
///
/// The underlying options represent:
/// * [`Error`](ErrorType::Error) - error at the user side (wrong usage of the program).
/// * [`Internal`](ErrorType::Internal) - program's internal logic error, means a critical bug is present.
/// * [`Hint`](ErrorType::Hint) - a suggestion for the user how to fix the error.
/// * [`Note`](ErrorType::Note) - any additional information that the user should know.
/// * [`BareMessage`](ErrorType::BareMessage) - a non-error message, which does not classify as hint nor note.
///
/// # Usage
/// The errors are added on a stack, so when adding an error with a hint, the hint should be added before the error.
pub enum ErrorType {
    Internal,
    Error,
    Note,
    Hint,
    BareMessage,
}

#[derive(Debug)]
/// Quackpack error type, contains a stack of messages for the user.
pub struct QuackError {
    error: BoxError,
    display_place: DisplayPlace,
}

impl QuackError {
    /// Creates a [`QuackError`] with a single error message.
    pub fn new<T>(err: T) -> Self
    where
        T: Error + Send + Sync + 'static,
    {
        let dyn_err: &dyn Error = &err;
        let display_place = if let Some(clap_err) = dyn_err.downcast_ref::<clap::Error>()
            && !clap_err.use_stderr()
        {
            DisplayPlace::StdOut
        } else {
            DisplayPlace::StdErr
        };
        Self {
            error: Box::new(err),
            display_place,
        }
    }

    /// Create a [`QuackError`] from a message.
    pub fn message<M: fmt::Display>(m: M) -> Self {
        Self::new(MessageError(m.to_string().into()))
    }

    /// Add a context to this [`QuackError`].
    pub fn context<C: Error + Send + Sync + 'static>(self, context: C) -> Self {
        let context = ContextError {
            context: Box::new(context),
            error: self,
        };
        Self::new(context)
    }

    /// Try to downcast a reference in errors' chain.
    /// Any [`ContextError`] is silently *ignored* (it's treated as its context).
    pub fn downcast_ref_in_chain<T: Error + 'static>(&self) -> Option<&T> {
        self.sources()
            .filter_map(ErrorExt::context_aware_downcast_ref)
            .next()
    }

    /// Check, if errors' chain contains `T`.
    /// Any [`ContextError`] is silently *ignored* (it's treated as its context).
    pub fn has_in_chain<T: Error + 'static>(&self) -> bool {
        self.downcast_ref_in_chain::<T>().is_some()
    }

    /// The current root error.
    pub fn error(&self) -> &(dyn Error + Send + Sync + 'static) {
        self.error.as_ref()
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
    pub fn sources(&self) -> Sources<'_> {
        Sources {
            current: Some(self.error.as_ref()),
        }
    }

    /// Add a note on top of this errors' chain.
    pub fn add_note<C: Into<Cow<'static, str>>>(self, note: C) -> Self {
        self.context(NoteMessage::new(note))
    }

    /// Add a hint on top of this errors' chain.
    pub fn add_hint<C: Into<Cow<'static, str>>>(self, hint: C) -> Self {
        self.context(HintMessage::new(hint))
    }

    /// Create a new note [`QuackError`].
    pub fn note<C: Into<Cow<'static, str>>>(note: C) -> Self {
        NoteMessage::new(note).into()
    }

    /// Create a new hint [`QuackError`].
    pub fn hint<C: Into<Cow<'static, str>>>(hint: C) -> Self {
        HintMessage::new(hint).into()
    }
}

macro_rules! create_messages_errors {
    ($($name:ident)*) => {
        $(
            #[derive(PartialEq, Eq)]
            /// A message wrapped in an [`Error`]-like struct.
            pub struct $name(pub Cow<'static, str>);

            impl $name {
                #[doc = concat!("Create a new `[", stringify!($name), "]` from a message.")]
                pub fn new(message: impl Into<Cow<'static, str>>) -> Self {
                    Self(message.into())
                }
            }

            impl fmt::Debug for $name {
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                    fmt::Debug::fmt(&self.0, f)
                }
            }

            impl fmt::Display for $name {
                fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                    fmt::Display::fmt(&self.0, f)
                }
            }

            impl Error for $name {}
        )*
    };
}

create_messages_errors!(MessageError NoteMessage HintMessage InternalError BareMessage);

#[derive(Debug)]
/// A context with an error.
///
/// Displays context, but returns `error` as a source.
struct ContextError {
    context: BoxError,
    error: QuackError,
}

impl fmt::Display for ContextError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&self.context, f)
    }
}

impl Error for ContextError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        Some(self.error.error())
    }
}

/// Iterator over `.source()`s of an [`Error`].
pub struct Sources<'a> {
    current: Option<&'a (dyn Error + 'static)>,
}

impl<'a> Iterator for Sources<'a> {
    type Item = &'a (dyn Error + 'static);

    fn next(&mut self) -> Option<Self::Item> {
        let current = self.current?;
        self.current = current.source();
        Some(current)
    }
}

impl fmt::Display for QuackError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let errors = self.sources().collect::<Vec<_>>();
        for (i, x) in errors.iter().enumerate() {
            if i < errors.len() - 1 {
                writeln!(f, "{x}")?;
            } else {
                write!(f, "{x}")?;
            }
        }
        Ok(())
    }
}

/// Returns with a single error message [`QuackError`] wrapped in [`Err`].
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

/// Returns with a single internal error message [`QuackError`] wrapped in [`Err`].
#[macro_export]
macro_rules! qp_bail_internal {
    ($msg:literal $(,)?) => {
        return Err($crate::qp_internal!($msg))
    };
    ($err:expr $(,)?) => {
        return Err($crate::qp_internal!($err))
    };
    ($fmt:expr, $($args:tt)*) => {
        return Err($crate::qp_internal!($fmt, $($args)*))
    };
}

/// Creates a single error message [`QuackError`].
#[macro_export]
macro_rules! qp_err {
    ($msg:literal $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::new($crate::util::error::MessageError::new(static_msg))
        } else {
            $crate::QuackError::message(format!($msg))
        }
    }};
    ($err:expr $(,)?) => {
        $crate::QuackError::from($err)
    };
    ($fmt:expr, $($args:tt)*) => {
        $crate::QuackError::message(format!($fmt, $($args)*))
    };
}

/// Creates a single internal error message [`QuackError`].
#[macro_export]
macro_rules! qp_internal {
    ($msg:literal $(,)?) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::from($crate::util::error::InternalError::new(static_msg))
        } else {
            $crate::QuackError::from($crate::util::error::InternalError::new(format!($msg)))
        }
    }};
    ($err:expr $(,)?) => {
        $crate::QuackError::from($err)
    };
    ($fmt:expr, $($args:tt)*) => {
         $crate::QuackError::from($crate::util::error::InternalError::new(format!($fmt, $($args)*)))
    };
}

/// A trait for adding contexts to results.
pub trait QuackResultContext<T, E> {
    /// Transforms self into a [`QuackResult`] and adds context.
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display;

    /// Transforms self into a [`QuackResult`] and adds internal error context.
    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display;

    /// Transforms self into a [`QuackResult`], computes and adds context.
    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C;

    /// Transforms self into a [`QuackResult`], computes and adds internal error context.
    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C;
}

impl<T, E> QuackResultContext<T, E> for Result<T, E>
where
    E: Into<QuackError>,
{
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display,
    {
        self.map_err(|e| e.into().context(MessageError::new(ctx.to_string())))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display,
    {
        self.map_err(|e| e.into().context(InternalError::new(ctx.to_string())))
    }

    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C,
    {
        self.map_err(|e| e.into().context(MessageError::new(ctx().to_string())))
    }

    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C,
    {
        self.map_err(|e| e.into().context(InternalError::new(ctx().to_string())))
    }
}

impl<T> QuackResultContext<T, QuackError> for Option<T> {
    fn context<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display,
    {
        self.ok_or_else(|| QuackError::message(ctx.to_string()))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where
        C: fmt::Display,
    {
        self.ok_or_else(|| InternalError::new(ctx.to_string()).into())
    }

    fn with_context<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C,
    {
        self.ok_or_else(|| QuackError::message(ctx().to_string()))
    }

    fn with_context_internal<C, F>(self, ctx: F) -> QuackResult<T>
    where
        C: fmt::Display,
        F: FnOnce() -> C,
    {
        self.ok_or_else(|| InternalError::new(ctx().to_string()).into())
    }
}

impl<E> From<E> for QuackError
where
    E: Error + 'static + Send + Sync,
{
    fn from(value: E) -> Self {
        Self::new(value)
    }
}

pub trait ErrorExt {
    /// Get the [`ErrorType`] of this error.
    fn error_type(&self) -> ErrorType;

    /// Downcast self to a reference, but if self is [`ContextError`], then also attempt to
    /// downcast a [`ContextError::context`].
    fn context_aware_downcast_ref<T: Error + 'static>(&self) -> Option<&T>;

    /// Check, if self is T, but if self is [`ContextError`], then also attempt to
    /// check a [`ContextError::context`].
    fn context_aware_is<T: Error + 'static>(&self) -> bool {
        self.context_aware_downcast_ref::<T>().is_some()
    }
}

impl ErrorExt for dyn Error + 'static {
    fn error_type(&self) -> ErrorType {
        if self.context_aware_is::<InternalError>() {
            ErrorType::Internal
        } else if self.context_aware_is::<NoteMessage>() {
            ErrorType::Note
        } else if self.context_aware_is::<HintMessage>() {
            ErrorType::Hint
        } else if let Some(clap_err) = self.context_aware_downcast_ref::<clap::Error>()
            && !clap_err.use_stderr()
        {
            ErrorType::BareMessage
        } else if self.context_aware_is::<BareMessage>() {
            ErrorType::BareMessage
        } else {
            ErrorType::Error
        }
    }

    fn context_aware_downcast_ref<T: Error + 'static>(&self) -> Option<&T> {
        self.downcast_ref::<T>().or_else(|| {
            self.downcast_ref::<ContextError>()
                .and_then(|context| context.context.downcast_ref::<T>())
        })
    }
}

impl ErrorExt for dyn Error + Send + 'static {
    fn error_type(&self) -> ErrorType {
        <dyn Error + 'static>::error_type(self)
    }

    fn context_aware_downcast_ref<T: Error + 'static>(&self) -> Option<&T> {
        <dyn Error + 'static>::context_aware_downcast_ref(self)
    }
}

impl ErrorExt for dyn Error + Send + Sync + 'static {
    fn error_type(&self) -> ErrorType {
        <dyn Error + 'static>::error_type(self)
    }

    fn context_aware_downcast_ref<T: Error + 'static>(&self) -> Option<&T> {
        <dyn Error + 'static>::context_aware_downcast_ref(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn context_downcasting() {
        let error: QuackResult<()> = Err(QuackError::new(std::io::Error::other("my error")))
            .context("my context")
            .context_internal("my internal context");
        let error = error.unwrap_err().add_note("my note").add_hint("my hint");

        assert_eq!(
            error.sources().map(ToString::to_string).collect::<Vec<_>>(),
            [
                "my hint",
                "my note",
                "my internal context",
                "my context",
                "my error"
            ]
        );
        assert!(error.has_in_chain::<InternalError>());
        assert!(error.has_in_chain::<MessageError>());
        assert!(error.has_in_chain::<std::io::Error>());
        assert!(error.has_in_chain::<NoteMessage>());
        assert!(error.has_in_chain::<HintMessage>());
        let types = error
            .sources()
            .map(ErrorExt::error_type)
            .collect::<Vec<_>>();

        assert_eq!(
            types,
            [
                ErrorType::Hint,
                ErrorType::Note,
                ErrorType::Internal,
                ErrorType::Error,
                ErrorType::Error
            ]
        );
    }
}
