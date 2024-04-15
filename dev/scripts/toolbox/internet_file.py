import http.client
import pathlib
import requests
import os
import http

from scripts.toolbox.helpers import bash_command, exit_with_error, log_info


class InternetFile:

    def __init__(self, path, resource_url, auth=(), after_download=None):
        self.path = path
        self.resource_url = resource_url
        self.auth = auth
        self.after_download = after_download

    def download(self, force=False):
        if force or not os.path.exists(self.path):
            try:
                print(f"Downloading {self.path}...")
                res = requests.get(self.resource_url, stream=True, auth=self.auth)

                if res.status_code != 200:
                    exit_with_error(
                        f"Cannot download: {self.resource_url}, status_code = {res.status_code}, meaning: {http.client.responses[res.status_code]}"
                    )

                with open(self.path, "wb") as f:
                    for chunk in res.iter_content(chunk_size=8192):
                        f.write(chunk)
                print(f"Done downloading {self.path} from {self.resource_url}")

                if self.after_download:
                    self.after_download(self)

            except requests.exceptions.HTTPError as e:
                exit_with_error(e)


def callback_unTARXZ_and_remove(res: InternetFile):
    log_info(f"Untarrxzing {res.path}...")
    res.path
    bash_command(
        f"tar -xf {res.path} --directory={pathlib.Path(res.path).parent.absolute()}"
    )
