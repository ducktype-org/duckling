use std::{error::Error, fmt, num::TryFromIntError};

use crate::QuackResult;

/// Quackpack error type, contains a stack of messages for the user.
pub struct QuackError {
    inner: Vec<QpErrorType>,
    exit_code: i32,
}

pub enum QpErrorType {
    Error(Box<dyn AsRef<str>>),
    Internal(Box<dyn AsRef<str>>),
    Hint(Box<dyn AsRef<str>>),
    Note(Box<dyn AsRef<str>>),
}

impl QuackError {
    pub fn new() -> Self {
        Self { inner: Vec::new(), exit_code: 0 }
    }

    pub fn error<T>(err: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        Self { inner: vec![QpErrorType::Error(Box::new(err))], exit_code: 1}
    }

    pub fn internal<T>(err: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        Self { inner: vec![QpErrorType::Internal(Box::new(err))], exit_code: 1}
    }

    pub fn add_hint<T>(mut self: Self, hint: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        self.inner.push(QpErrorType::Hint(Box::new(hint)));
        self
    }

    pub fn add_note<T>(mut self: Self, note: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        self.inner.push(QpErrorType::Note(Box::new(note)));
        self
    }

    pub fn context<T>(mut self: Self, ctx: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        self.inner.push(QpErrorType::Error(Box::new(ctx)));
        self
    }

    pub fn context_internal<T>(mut self: Self, ctx: T) -> Self
    where T: AsRef<str> + Sized + 'static {
        self.inner.push(QpErrorType::Internal(Box::new(ctx)));
        self
    }

    pub fn change_exit_code(mut self: Self, new_code: i32) -> Self {
        self.exit_code = new_code;
        self
    }

    pub fn exit_code(&self) -> i32 {
        self.exit_code
    }

    pub fn stack(&self) -> &[QpErrorType] {
        &self.inner
    }
}

impl fmt::Display for QuackError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (i, x) in self.inner.iter().rev().enumerate() {
            if i < self.inner.len() - 1 {
                writeln!(f, "{x}")?;
            }
            else {
                write!(f, "{x}")?;
            }
        }
        Ok(())
    }
}

impl fmt::Debug for QuackError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (i, x) in self.inner.iter().rev().enumerate() {
            if i < self.inner.len() - 1 {
                writeln!(f, "{x}")?;
            }
            else {
                write!(f, "{x}")?;
            }
        }
        Ok(())
    }
}

impl fmt::Display for QpErrorType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            QpErrorType::Error(error) => write!(f, "{}", error.as_ref().as_ref())?,
            QpErrorType::Internal(error) => write!(f, "{}", error.as_ref().as_ref())?,
            QpErrorType::Hint(hint) => write!(f, "{}", hint.as_ref().as_ref())?,
            QpErrorType::Note(note) => write!(f, "{}", note.as_ref().as_ref())?,
        }
        Ok(())
    }
}

impl fmt::Debug for QpErrorType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            QpErrorType::Error(error) => write!(f, "{}", error.as_ref().as_ref())?,
            QpErrorType::Internal(error) => write!(f, "{}", error.as_ref().as_ref())?,
            QpErrorType::Hint(hint) => write!(f, "{}", hint.as_ref().as_ref())?,
            QpErrorType::Note(note) => write!(f, "{}", note.as_ref().as_ref())?,
        }
        Ok(())
    }
}

impl Error for QuackError {}

#[macro_export]
macro_rules! qp_bail {
    ($msg:literal) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            return Err($crate::QuackError::error(static_msg));
        }
        else {
            return Err($crate::QuackError::error(format!($msg)));
        }
    }};
    ($err:expr) => {{
        return Err($err.into());
    }};
    ($fmt:expr, $($args:expr),*) => {
        return Err($crate::QuackError::error(format!($fmt, $($args),*)))
    };
}

#[macro_export]
macro_rules! qp_err {
    ($msg:expr) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::error(static_msg)
        }
        else {
            $crate::QuackError::error(format!($msg))
        }
    }};
    ($err:expr) => {{
        $err.into()
    }};
    ($fmt:expr, $($args:expr),*) => {
        $crate::QuackError::error(format!($fmt, $($args),*))
    };
}

#[macro_export]
macro_rules! qp_bail_internal {
    ($msg:expr) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            return Err($crate::QuackError::internal(static_msg));
        }
        else {
            return Err($crate::QuackError::internal(format!($msg)));
        }
    }};
    ($err:expr) => {{
        return Err($crate::QuackError::internal(format!("{$err}")));
    }};
    ($fmt:expr, $($args:expr),*) => {
        return Err($crate::QuackError::internal(format!($fmt, $($args),*)))
    };
}

#[macro_export]
macro_rules! qp_internal {
    ($msg:expr) => {{
        let args = format_args!($msg);
        if let Some(static_msg) = args.as_str() {
            $crate::QuackError::internal(static_msg)
        }
        else {
            $crate::QuackError::internal(format!($msg))
        }
    }};
    ($err:expr) => {{
        $crate::QuackError::internal(format!("{$err}"))
    }};
    ($fmt:expr, $($args:expr),*) => {
        $crate::QuackError::internal(format!($fmt, $($args),*))
    };
}

pub trait QuackResultContext<T, E> {
    fn context<C>(self: Self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static;

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static;

    fn with_context<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C;

    fn with_context_internal<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C;
}

impl<T, E> QuackResultContext<T, E> for Result<T, E>
where E: Into<QuackError> {
    fn context<C>(self: Self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static {
        self.map_err(|e| e.into().context(ctx))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static {
        self.map_err(|e| e.into().context_internal(ctx))
    }
    
    fn with_context<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C {
        self.map_err(|e| e.into().context(ctx()))
    }
    
    fn with_context_internal<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C {
        self.map_err(|e| e.into().context_internal(ctx()))
    }
}

impl<T> QuackResultContext<T, QuackError> for Option<T> {
    fn context<C>(self: Self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static {
        self.ok_or_else(|| QuackError::error(ctx))
    }

    fn context_internal<C>(self, ctx: C) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static {
        self.ok_or_else(|| QuackError::internal(ctx))
    }
    
    fn with_context<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C {
        self.ok_or_else(|| QuackError::error(ctx()))
    }
    
    fn with_context_internal<C, F>(self: Self, ctx: F) -> QuackResult<T>
    where C: AsRef<str> + Sized + 'static,
    F: FnOnce() -> C {
        self.ok_or_else(|| QuackError::internal(ctx()))
    }
}

impl From<std::io::Error> for QuackError {
    fn from(value: std::io::Error) -> Self {
        QuackError::error(format!("{value}"))
    }
}

impl From<toml::de::Error> for QuackError {
    fn from(value: toml::de::Error) -> Self {
        QuackError::error(format!("{value}"))
    }
}

impl From<serde_yaml_ng::Error> for QuackError {
    fn from(value: serde_yaml_ng::Error) -> Self {
        QuackError::error(format!("{value}"))
    }
}

impl From<TryFromIntError> for QuackError {
    fn from(value: TryFromIntError) -> Self {
        QuackError::error(format!("{value}"))
    }
}

impl From<clap::Error> for QuackError {
    fn from(value: clap::Error) -> Self {
        let err = QuackError::error(format!("{}", value.render().ansi()));
        if matches!(value.kind(), clap::error::ErrorKind::DisplayHelp) {
            err.change_exit_code(0)
        }
        else {
            err
        }
    }
}

impl From<String> for QuackError {
    fn from(value: String) -> Self {
        QuackError::error(value)
    }
}
