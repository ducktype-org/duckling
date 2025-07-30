from quackpack.util.types.errors import QuackPackError


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
