from quackpack.util.errors import QuackPackError


class UninitializedClientError(QuackPackError):
    def __init__(self):
        super().__init__("Fetcher: Ducknest client or metadata cache not initialized")


class MissingEventLoopError(QuackPackError):
    def __init__(self):
        super().__init__("There is no running event loop")


class FailedRequestError(QuackPackError):
    def __init__(self):
        super().__init__("Request failed")
