def distance(lhs: str, rhs: str) -> int:
    """
    Get the Levenshtein (editing) distance between `lhs` and `rhs`.
    ----
    Args:
    - `lhs`, `rhs`: strings to compare.
    ----
    Returns:
    - `int`: distance between `lhs` and `rhs`.
    """
    # https://en.wikipedia.org/wiki/Levenshtein_distance#Iterative_with_two_matrix_rows
    n = len(rhs)
    m = len(lhs)
    v0 = list(range(n + 1))
    v1 = [0] * (n + 1)

    for i in range(m):
        v1[0] = i + 1
        for j in range(n):
            deletion_cost = v0[j + 1] + 1
            insertion_cost = v1[j] + 1
            substitution_cost = v0[j] + (lhs[i] != rhs[j])
            v1[j + 1] = min(deletion_cost, insertion_cost, substitution_cost)
        v1, v0 = v0, v1
    return v0[n]
