import http
import http.client
import pathlib
import sys

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
                    total_length = int(res.headers.get('content-length'))
                    if total_length is None:
                        log_info(f"Cannot display a progress bar during downloading of {self.path}...")
                    total_length_mib = round(total_length / 1024 / 1024, 2)

                    written = 0
                    for chunk in res.iter_content(chunk_size=8192):
                        f.write(chunk)

                        # If we know the full length, then write a progress bar
                        if total_length is not None: # content length header exists
                            written += len(chunk)
                            written_mib = round(written / 1024 / 1024, 2)
                            written_formatted = f'{{:>{len(str(total_length_mib))}}}'.format(written_mib)
                            # Force a carriage return and rewrite the line
                            sys.stdout.write(f"\r{self.path}: {written_formatted} / {total_length_mib} MiB")
                            sys.stdout.flush()

                    if total_length is not None:
                        sys.stdout.write('\n')
                        sys.stdout.flush()


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
