class QuackPackError(Exception):
    def __init__(self, reason: str | Exception) -> None:
        super().__init__(reason)
