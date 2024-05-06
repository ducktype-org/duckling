import http
import http.client
import pathlib

import requests

from scripts.toolbox.helpers import bash_command, exit_with_error, log_info


class InternetFile:

    def __init__(self, path, resource_url, auth=(), after_download=()):
        self.path = pathlib.Path(path)
        self.resource_url = resource_url
        self.auth = auth
        self.after_download_callbacks = after_download
        self.parent_dir = self.path.parent.absolute()

    def download(self, force=False):
        if force or not self.path.exists():
            try:
                log_info(f"Downloading {self.path}...")
                res = requests.get(self.resource_url, stream=True, auth=self.auth)

                if res.status_code != 200:
                    exit_with_error(
                        f"Cannot download: {self.resource_url}, status_code = {res.status_code}, meaning: {http.client.responses[res.status_code]}"
                    )

                with open(self.path, "wb") as f:
                    for chunk in res.iter_content(chunk_size=8192):
                        f.write(chunk)
                log_info(f"Done downloading {self.path} from {self.resource_url}")

                for callback in self.after_download_callbacks:
                    callback[0](self, *callback[1:])

            except requests.exceptions.HTTPError as e:
                exit_with_error(e)
        else:
            log_info(f"File {self.path} already exits. Skiped.")


def callback_unTAR(res: InternetFile):
    log_info(f"Untarring {res.path}...")
    bash_command(f"tar -xf {res.path} --directory={res.parent_dir}")


def callback_remove(res: InternetFile, file_path: str):
    log_info(f"Removing {file_path}...")
    bash_command(f"rm -r {res.parent_dir / file_path}")


def callback_move(res: InternetFile, path_from: str, path_to: str):
    log_info(f"Moving {res.parent_dir / path_from} to {res.parent_dir / path_to}...")
    bash_command(f"mv {res.parent_dir / path_from} {res.parent_dir / path_to}")


def callback_chmod(res: InternetFile, path, mode):
    log_info(f"Chmoding {res.parent_dir / path} to {mode}...")
    bash_command(f"chmod {mode} {res.parent_dir / path}")
