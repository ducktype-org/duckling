import random

PROB = 30

def random_identifier(length: int) -> str:
    letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    chars = "1234567890_"
    return random.choice(letters) + ''.join(random.choice(chars+letters) for _ in range(length-1))

