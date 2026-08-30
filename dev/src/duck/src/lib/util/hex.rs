//! Module containing hex encoding (and, maybe in the future, decoding too).

/// Encode data as a hex string.
pub fn encode(data: impl AsRef<[u8]>) -> String {
    encode_inner(data.as_ref())
}

/// Helper for [`encode`].
fn encode_inner(data: &[u8]) -> String {
    let mut encoded = "".to_owned();
    for byte in data {
        let formatted = format_byte(*byte);
        encoded.push(formatted[0]);
        encoded.push(formatted[1]);
    }
    encoded
}

/// Hex encode a single byte.
#[inline]
fn format_byte(byte: u8) -> [char; 2] {
    [format_hex(byte >> 4), format_hex(byte % 16)]
}

/// Hex encode a single hexadecimal digit.
#[inline]
fn format_hex(hex: u8) -> char {
    match hex {
        0 => '0',
        1 => '1',
        2 => '2',
        3 => '3',
        4 => '4',
        5 => '5',
        6 => '6',
        7 => '7',
        8 => '8',
        9 => '9',
        10 => 'a',
        11 => 'b',
        12 => 'c',
        13 => 'd',
        14 => 'e',
        15 => 'f',
        _ => unreachable!("`{hex}` is not a hex value?"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn format_test() {
        assert_eq!(format_byte(255), ['f', 'f']);
        assert_eq!(format_byte(0), ['0', '0']);
        assert_eq!(format_byte(16), ['1', '0']);
        assert_eq!(format_byte(31), ['1', 'f']);
        assert_eq!(format_byte(128), ['8', '0']);
    }
}
