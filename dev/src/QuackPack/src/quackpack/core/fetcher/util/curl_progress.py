import pycurl
from rich import console, progress


class CurlProgress:
    """
    A class for managing and displaying real-time download progress using pycurl and Rich.

    This class tracks the progress of multiple concurrent pycurl downloads, updating a live
    progress bar for each and a summary counter upon completion.

    :param blob_progress: A Rich Progress instance used for displaying individual download tasks.
    :type blob_progress: rich.progress.Progress
    :param completed_progress: A Rich Progress instance used for tracking completed downloads.
    :type completed_progress: rich.progress.Progress
    :param packages_count: Total number of packages to be downloaded.
    :type packages_count: int | None
    """

    def __init__(
        self,
        blob_progress: progress.Progress,
        completed_progress: progress.Progress,
        packages_count: int | None = None,
    ) -> None:
        self.blob_progress = blob_progress
        self.completed_progress = completed_progress
        self.completed_task_id = self.completed_progress.add_task("Downloaded", total=packages_count)
        self.tasks: dict[pycurl.Curl, progress.TaskID] = {}
        self.total_downloaded_bytes: dict[pycurl.Curl, int] = {}

    @staticmethod
    def make_blob_progress(console: console.Console) -> progress.Progress:
        """
        Create a progress display for individual blob downloads.

        :return: A configured Progress instance for displaying blob download progress.
        :rtype: rich.progress.Progress
        """

        return progress.Progress(
            progress.SpinnerColumn(),
            progress.TextColumn("{task.description}"),
            progress.BarColumn(),
            progress.TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
            "eta",
            progress.TimeRemainingColumn(),
            progress.TextColumn("{task.fields[message]}"),
            console=console,
            transient=True,
        )

    @staticmethod
    def make_completed_progress(console: console.Console) -> progress.Progress:
        """
        Create a progress display to track the number of completed package downloads.

        :return: A configured Progress instance for displaying overall completion.
        :rtype: rich.progress.Progress
        """

        return progress.Progress(
            progress.SpinnerColumn(),
            progress.BarColumn(),
            progress.TextColumn("{task.description} {task.completed} of {task.total} packages"),
            console=console,
            transient=True,
        )

    def add_task(self, curl: pycurl.Curl, description: str) -> None:
        """
        Add a new task to the blob progress display for a given Curl download.

        :param curl: The Curl object associated with the task.
        :type curl: pycurl.Curl
        :param description: Description of the task (e.g., URL being downloaded).
        :type description: str
        """

        task_id = self.blob_progress.add_task(
            description=description, total=0, message="Starting download..."
        )
        self.tasks[curl] = task_id
        self.total_downloaded_bytes[curl] = 0

    def remove_task(self, curl: pycurl.Curl) -> None:
        """
        Remove a completed Curl download task and update the completed counter.

        :param curl: The Curl object to remove.
        :type curl: pycurl.Curl
        """

        if curl in self.tasks:
            task_id = self.tasks[curl]
            self.blob_progress.update(task_id, visible=False)
            self.completed_progress.advance(self.completed_task_id, 1)
            del self.tasks[curl]
            del self.total_downloaded_bytes[curl]

    def status(
        self, curl: pycurl.Curl, download_t: int, download_d: int, _upload_t: int, _upload_d: int
    ) -> int:
        """
        Callback function for reporting pycurl download progress.

        This is used by pycurl to report download status, and updates Rich's Progress UI accordingly.

        :param curl: The Curl object being tracked.
        :type curl: pycurl.Curl
        :param download_t: Total bytes expected to download.
        :type download_t: int
        :param download_d: Number of bytes downloaded so far.
        :type download_d: int
        :param _upload_t: Total bytes expected to upload (unused).
        :type _upload_t: int
        :param _upload_d: Number of bytes uploaded so far (unused).
        :type _upload_d: int
        :return: 0 to continue download, non-zero to abort.
        :rtype: int
        """

        if curl not in self.tasks:
            return 0

        task_id = self.tasks[curl]

        if download_t > 0 and self.blob_progress.tasks[task_id].total != download_t:
            self.blob_progress.update(task_id, total=download_t)

        if download_d > self.total_downloaded_bytes[curl]:
            self.blob_progress.update(
                task_id=task_id, completed=download_d, message=f"Downloaded {download_d} bytes"
            )
            self.total_downloaded_bytes[curl] = download_d

        return 0
