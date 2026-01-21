use crate::{QuackResult, QuackResultContext, qp_internal};

/// Extract a single item stored in this vector.
pub fn extract_single_item_from_vec<T>(vec: Vec<T>) -> QuackResult<T> {
    let [single] = vec
        .try_into()
        .map_err(|_| qp_internal!("expected a single item in a vector"))?;
    Ok(single)
}

/// Unpack a vec returned by [`TokioScope`](async_scoped::Scope).
pub fn unpack_tokio_scoped_vector<T>(
    vec: Vec<Result<T, tokio::task::JoinError>>,
) -> QuackResult<Vec<T>> {
    vec.into_iter()
        .collect::<Result<_, _>>()
        .context_internal("thread panicked")
}
