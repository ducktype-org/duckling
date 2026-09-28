//! Reading the `DT_SONAME` of a shared object, which is the name the dynamic loader knows it by.

use std::path::Path;

const ELF_MAGIC: &[u8; 4] = b"\x7fELF";
const SHT_DYNAMIC: u32 = 6;
const DT_NULL: u64 = 0;
const DT_SONAME: u64 = 14;

/// Little-endian 64-bit reads, the only layout this is needed for.
struct Reader<'a>(&'a [u8]);

impl Reader<'_> {
    fn u16(&self, at: usize) -> Option<u16> {
        Some(u16::from_le_bytes(self.0.get(at..at + 2)?.try_into().ok()?))
    }

    fn u32(&self, at: usize) -> Option<u32> {
        Some(u32::from_le_bytes(self.0.get(at..at + 4)?.try_into().ok()?))
    }

    fn u64(&self, at: usize) -> Option<u64> {
        Some(u64::from_le_bytes(self.0.get(at..at + 8)?.try_into().ok()?))
    }

    fn c_str(&self, at: usize) -> Option<String> {
        let bytes = self.0.get(at..)?;
        let end = bytes.iter().position(|&b| b == 0)?;
        String::from_utf8(bytes[..end].to_vec()).ok()
    }
}

/// The `DT_SONAME` of a 64-bit little-endian ELF shared object, if it has one.
pub fn soname_of(bytes: &[u8]) -> Option<String> {
    let r = Reader(bytes);
    // ELFCLASS64, ELFDATA2LSB.
    if bytes.get(..4)? != ELF_MAGIC || *bytes.get(4)? != 2 || *bytes.get(5)? != 1 {
        return None;
    }
    let section_offset = usize::try_from(r.u64(0x28)?).ok()?;
    let section_size = usize::from(r.u16(0x3a)?);
    let section_count = usize::from(r.u16(0x3c)?);

    let section = |index: usize| section_offset + index * section_size;
    for index in 0..section_count {
        let header = section(index);
        if r.u32(header + 4)? != SHT_DYNAMIC {
            continue;
        }
        let dynamic_offset = usize::try_from(r.u64(header + 0x18)?).ok()?;
        let dynamic_size = usize::try_from(r.u64(header + 0x20)?).ok()?;
        // The dynamic section's `sh_link` names its string table.
        let strtab = section(usize::try_from(r.u32(header + 0x28)?).ok()?);
        let strtab_offset = usize::try_from(r.u64(strtab + 0x18)?).ok()?;

        let mut entry = dynamic_offset;
        while entry + 16 <= dynamic_offset + dynamic_size {
            let tag = r.u64(entry)?;
            if tag == DT_NULL {
                break;
            }
            if tag == DT_SONAME {
                let name = usize::try_from(r.u64(entry + 8)?).ok()?;
                return r.c_str(strtab_offset + name);
            }
            entry += 16;
        }
    }
    None
}

/// [`soname_of`] for the file at `path`. A GNU linker script (`libm.so` often is one) names the
/// real shared object instead, so the first listed file with a soname is used.
pub fn soname_of_file(path: &Path) -> Option<String> {
    let bytes = std::fs::read(path).ok()?;
    if bytes.starts_with(ELF_MAGIC) {
        return soname_of(&bytes);
    }
    let script = String::from_utf8(bytes).ok()?;
    let dir = path.parent().unwrap_or(Path::new("."));
    script
        .split(|c: char| c.is_whitespace() || c == '(' || c == ')')
        .filter(|token| token.contains(".so"))
        .find_map(|token| {
            let listed = Path::new(token);
            let listed = if listed.is_absolute() {
                listed.to_path_buf()
            } else {
                dir.join(listed)
            };
            let bytes = std::fs::read(listed).ok()?;
            soname_of(&bytes)
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn follows_linker_scripts() {
        let dir = tempfile::tempdir().unwrap();
        let script = dir.path().join("libfake.so");
        std::fs::write(
            &script,
            "/* GNU ld script */\nGROUP ( /nonexistent/libfake.so.1 )\n",
        )
        .unwrap();
        assert_eq!(soname_of_file(&script), None);
    }

    #[test]
    fn rejects_non_elf() {
        assert_eq!(soname_of(b"not an elf file at all, just some bytes"), None);
    }

    #[test]
    #[cfg(all(target_os = "linux", target_arch = "x86_64"))]
    fn reads_libc_soname() {
        let candidates = [
            "/lib/x86_64-linux-gnu/libc.so.6",
            "/usr/lib/x86_64-linux-gnu/libc.so.6",
            "/usr/lib/libc.so.6",
            "/usr/lib64/libc.so.6",
        ];
        let Some(path) = candidates.iter().map(Path::new).find(|p| p.exists()) else {
            return;
        };
        assert_eq!(soname_of_file(path).as_deref(), Some("libc.so.6"));
    }
}
