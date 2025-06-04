from quackpack.util.errors import QuackPackError


class UninitializedClientError(QuackPackError):
    """
    Exception raised when the Ducknest client or metadata cache has not been initialized.
    """

    def __init__(self):
        super().__init__("Fetcher: Ducknest client or metadata cache not initialized")


class MissingEventLoopError(QuackPackError):
    """
    Exception raised when there is no running event loop.
    """

    def __init__(self):
        super().__init__("There is no running event loop")


class FailedRequestError(QuackPackError):
    """
    Exception raised when an HTTP request has failed.
    """

    def __init__(self):
        super().__init__("Request failed")
