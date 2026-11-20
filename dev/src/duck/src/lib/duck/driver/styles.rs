// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use clap::builder::styling::{AnsiColor, Color, Style, Styles};

/// Get default colors (styles) for the `--help` messages.
pub const fn get_styles() -> Styles {
    Styles::styled()
        .usage(
            Style::new()
                .bold()
                .underline()
                .fg_color(Some(Color::Ansi(AnsiColor::Yellow))),
        )
        .header(
            Style::new()
                .bold()
                .underline()
                .fg_color(Some(Color::Ansi(AnsiColor::Magenta))),
        )
        .literal(Style::new().fg_color(Some(Color::Ansi(AnsiColor::BrightCyan))))
        .invalid(
            Style::new()
                .bold()
                .fg_color(Some(Color::Ansi(AnsiColor::Red))),
        )
        .error(
            Style::new()
                .bold()
                .fg_color(Some(Color::Ansi(AnsiColor::Red))),
        )
        .valid(
            Style::new()
                .bold()
                .underline()
                .fg_color(Some(Color::Ansi(AnsiColor::Green))),
        )
        .placeholder(Style::new().fg_color(Some(Color::Ansi(AnsiColor::BrightCyan))))
        .context(Style::new().fg_color(Some(Color::Ansi(AnsiColor::Blue))))
        .context_value(Style::new().fg_color(Some(Color::Ansi(AnsiColor::BrightBlue))))
}
