pub fn indent(text: &str, indentation: usize) -> String {
    text.lines()
        .map(|line| {
            if line.is_empty() {
                String::from("\n")
            } else {
                format!("{:indent$}{}\n", "", line, indent = indentation)
            }
        })
        .collect()
}
