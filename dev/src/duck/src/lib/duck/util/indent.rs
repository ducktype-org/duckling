/// Indent `text` with `indentation`.
pub fn indent(text: &str, indentation: usize) -> String {
    let ends_in_nl = text.ends_with('\n');
    let mut indented = text
        .lines()
        .map(|line| {
            if line.is_empty() {
                String::from("\n")
            } else {
                format!("{:indent$}{}\n", "", line, indent = indentation)
            }
        })
        .collect::<String>();
    if !ends_in_nl && indented.ends_with('\n') {
        let popped = indented.pop();
        debug_assert_eq!(popped, Some('\n'), "we haven't popped \\n");
    }
    indented
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn indent_tests() {
        assert_eq!("", indent("", 2), "indent ignores empty lines");
        assert_eq!("\n", indent("\n", 2), "indent should keep trailing newline");
        assert_eq!(
            "  ala
  ma
  kota",
            indent("ala\nma\nkota", 2)
        );
        assert_eq!(
            "  ala
  ma
  kota
",
            indent("ala\nma\nkota\n", 2)
        );
    }
}
