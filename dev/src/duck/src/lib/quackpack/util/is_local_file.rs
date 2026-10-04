use url::Url;

pub trait IsLocalFile {
    fn is_local_file(&self) -> bool;
}

impl IsLocalFile for Url {
    fn is_local_file(&self) -> bool {
        self.scheme() == "file"
    }
}
