use russcip::{Model, ProblemCreated, Variable, prelude::cons};

/// An extension of russcip::Model with functions for representing binary variable implications as constraints.
pub trait BinModelExt {
    /// A simple implication when => then.
    fn implies(&mut self, when: &Variable, then: &Variable);
    /// An implication of form when_all_1 ∧ ... ∧ when_all_n => then_any_1 ∨ ... ∨ then_any_m.
    fn implies_all_any<'a>(&mut self, when_all: Vec<&'a Variable>, then_any: Vec<&'a Variable>);
    /// An implication of form when => then_all_1 ∧ ... ∧ then_all_n.
    fn implies_one_all<'a>(&mut self, when: &Variable, then_all: Vec<&'a Variable>);
}

impl BinModelExt for Model<ProblemCreated> {
    fn implies(&mut self, when: &Variable, then: &Variable) {
        let constraint = cons().coef(when, 1.0).coef(then, -1.0).le(0.0);
        self.add(constraint);
    }

    fn implies_all_any(&mut self, when_all: Vec<&Variable>, then_any: Vec<&Variable>) {
        let when_all_len = when_all.len();
        let then_any_len = then_any.len();
        let constraint = cons()
            .coefs(when_all, vec![1.0; when_all_len])
            .coefs(then_any, vec![-1.0; then_any_len])
            .le((then_any_len - 1) as f64);
        self.add(constraint);
    }

    fn implies_one_all(&mut self, when: &Variable, then_all: Vec<&Variable>) {
        let then_all_len = then_all.len();
        let constraint = cons()
            .coef(when, then_all_len as f64)
            .coefs(then_all, vec![-1.0; then_all_len])
            .le(0.0);
        self.add(constraint);
    }
}
