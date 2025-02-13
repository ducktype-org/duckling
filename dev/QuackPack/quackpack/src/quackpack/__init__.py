import quackpack.logger
from quackpack.application.args import get_parser

_logger = quackpack.logger.get_logger(__name__)


def main() -> None:
    quackpack.logger.setup_logger()
    parser = get_parser()
    _logger.info("Hello from quackpack!")
    parser.parse_known_args()
