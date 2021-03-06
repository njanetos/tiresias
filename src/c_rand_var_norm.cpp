#include "c_rand_var_norm.h"

#include "c_logger.h"

c_rand_var_norm::c_rand_var_norm(size_t dim) {

    // set the dimension of the state space
    this->dim = dim;

    // the dimension of the optimization problem is the mean
    // vector plus the upper triangular Cholesky factorization of the
    // covariance matrix
    this->dim_prob = dim + dim*(dim+1)/2;

    // flag expensive computations as not yet complete
    inv_cov_is_computed = false;

    // prepare the raw data
    // first half of the vector is raw data
    // second half is optimization flags
    raw_data.resize(2*dim_prob);
}

void c_rand_var_norm::clone(c_rand_var_norm* parent) {
    this->dim = parent->dim;
    this->mean = parent->mean;
    this->cov = parent->cov;
    this->corr = parent->corr;
    this->var = parent->var;
    this->norm_factor = parent->norm_factor;
    this->det_cov = parent->det_cov;
    this->opt_flags = parent->opt_flags;
    this->raw_data = parent->raw_data;
}

real c_rand_var_norm::cdf(arma::mat& inequalities) {

    int infin[dim];
    double delta[dim];

    arma::Mat<real> ineq(inequalities);

    // Adjust the bounds
    ineq.col(0) = (ineq.col(0) - mean) / sqrt(cov.diag());
    ineq.col(1) = (ineq.col(1) - mean) / sqrt(cov.diag());

    bool lower, upper;

    for (size_t i = 0; i < dim; ++i) {
        lower = (ineq(i, 0) == -1*std::numeric_limits<real>::infinity());
        upper = (ineq(i, 1) == std::numeric_limits<real>::infinity());
        if (lower && upper) {
            infin[i] = -1;
        } else if (lower && !upper) {
            infin[i] = 0;
        } else if (!lower && upper) {
            infin[i] = 1;
        } else if (!lower && !upper) {
            infin[i] = 2;
        }
        delta[i] = 0;
    }

    // Build the correlation data
    double corr_raw[dim*(dim-1)/2];
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < i; ++j) {
            corr_raw[j + ((i-1)*i)/2] = corr(i, j);
        }
    }

    int nu = 0;
	int maxpts = 25000;
	double abseps = 0.001;
	double releps = 0;
	double error;
	double val;
	int inform;

    pmvnorm((int*) &dim, &nu, ineq.colptr(0), ineq.colptr(1), infin, corr_raw, delta, &maxpts, &abseps, &releps, &error, &val, &inform);

    return val;
}

std::vector<real> c_rand_var_norm::get_lower_bounds() const {
    std::vector<real> ret_val(get_opt_dim());

    size_t k = 0;
    for (size_t i = 0; i < get_dim(); ++i) {
        if (opt_flags[i]) {
            ret_val[k] = -HUGE_VAL;
            ++k;
        }
    }

    for (size_t i = get_dim(); i < get_dim_prob(); ++i) {
        if (opt_flags[i]) {
            // check whether i-dim is a triangular number!
            int calc_num = 8*(i-dim) + 1;
            int t = (int) sqrt(calc_num);
            ret_val[k] = t * t == calc_num ? 0 : -HUGE_VAL;
        }
    }

    return ret_val;
}

std::vector<real> c_rand_var_norm::get_upper_bounds() const {
    std::vector<real> ret_val(get_opt_dim());
    for (size_t i = 0; i < ret_val.size(); ++i) {
        ret_val[i] = HUGE_VAL;
    }
    return ret_val;
}

// TODO This needs to use the analytic formula.
arma::mat c_rand_var_norm::cdf_grad(arma::mat& inequalities) {

    c_rand_var_norm temp(this->dim);
    arma::mat res(dim_prob, 1);

    temp.dat_to_dist(&(this->raw_data[0]));
    temp.unpack();

    double base = temp.cdf(inequalities);

    for (size_t i = 0; i < dim+dim*(dim+1)/2; ++i) {
        temp.raw_data[i] += 0.001;
        temp.unpack();
        res(i) = (temp.cdf(inequalities) - base)/0.001;
        temp.raw_data[i] -= 0.001;
    }

    return res;
}

double c_rand_var_norm::div(c_rand_var_norm& var) {

    // Initialize the variable that will hold the divergence.
    double res = 0.0;

    // abs will hold the abscissa combination
    // tr_abs is the transformed position
    arma::Mat<double> abs(dim, 1);
    arma::Mat<double> tr_abs(dim, 1);

    // index is the current index
    size_t index[dim];

    // Initialize index to 0
    for (size_t i = 0; i < dim; ++i) {
        index[i] = 0;
    }

    // w_prod is the weight to multiply by
    double w_prod;

    // Loop through all possible abscissa combinations
    while (index[dim-1] < QUADRATURE_DIM) {

        // Start with weight = 1, find total weight by multiplying
        // Start reading in the abscissa values
        w_prod = 1.0;
        for (size_t i = 0; i < dim; ++i) {
            abs(i) = ABS[index[i]];
            w_prod *= WEIGHTS[index[i]];
        }

        // Advance the index by one
        ++index[0];
        for (size_t i = 0; i < dim-1; ++i) {
            if (index[i] >= QUADRATURE_DIM) {
                index[i] = 0;
                ++index[i+1];
            } else {
                break;
            }
        }

        // Make sure the weights aren't really small before performing computations.
        if (w_prod > WEIGHT_FLOOR) {
            // Transform the matrix to take into account correlation
            tr_abs = sqrt_two*ch*abs + mean;

            // Find the entropy and add onto the result, times the weight.
            res += ent(tr_abs, var)*w_prod;
        }

    }
    res *= gauss_factor;

    return res;
}

// TODO: this should use an analytic formula
arma::mat c_rand_var_norm::div_grad(c_rand_var_norm& oth) {

  c_rand_var_norm temp(this->dim);
  arma::mat res(dim_prob, 1);
  temp.clone(this);

  double base = temp.div(oth);

  for (size_t i = 0; i < dim+dim*(dim+1)/2; ++i) {
      temp.raw_data[i] += 0.001;
      temp.unpack();
      res(i) = (temp.div(oth) - base)/0.001;
      temp.dat_to_dist(&(this->raw_data[0]));
  }

  return res;
}

double c_rand_var_norm::ent(arma::mat& loc, c_rand_var_norm& var) {
    return std::log(pdf(loc)/var.pdf(loc));
}

arma::mat& c_rand_var_norm::inv_cov() {

    // Check if the inverse covariance matrix has already been computed.
    if (!inv_cov_is_computed) {

        // Compute the inverse.
        m_inv_cov = arma::inv(cov);

        // Flag it as completed.
        inv_cov_is_computed = true;

    }

    return m_inv_cov;
}

void c_rand_var_norm::pack() {

    for (size_t i = 0; i < dim; ++i) {
        raw_data[i] = mean(i);
    }

    size_t k = dim;
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j <= i; ++j) {
            raw_data[k] = cov(i, j);
            ++k;
        }
    }

    for (size_t i = 0; i < dim_prob; ++i) {
        raw_data[k] = opt_flags[i];
        ++k;
    }
}

double c_rand_var_norm::pdf(arma::mat& loc) {
    arma::mat expint = exp(-0.5*(loc - mean).t()*inv_cov()*(loc - mean));
    return norm_factor*expint(0);
}

arma::mat c_rand_var_norm::pdf_grad(arma::mat& loc) {
    return NULL;
}

void c_rand_var_norm::unpack() {

    // instantiate mean (can be directly written in).
    mean = arma::mat(&raw_data[0], dim, 1, true, true);

    // instantiate covariance matrix
    size_t k = dim;
    cov = arma::zeros<arma::mat>(dim, dim);
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j <= i; ++j) {
            cov(i, j) = raw_data[k];
            cov(j, i) = raw_data[k];
            ++k;
        }
    }

    // get the correlation matrix.
    corr = arma::diagmat(1/arma::sqrt(arma::diagmat(cov)))*cov*arma::diagmat(1/arma::sqrt(arma::diagmat(cov)));

    // find the remaining scalar values
    det_cov = det(cov);
    norm_factor = 1/(sqrt(pow(2*pi, dim)*det_cov));

    // flag inverse matrices as in need of computation
    inv_cov_is_computed = false;
    
    // compute the cholesky factorization
    ch = arma::chol(cov);

    // get optimization flags
    opt_flags.resize(dim_prob);

    for (size_t i = 0; i < get_dim_prob(); ++i) {
        opt_flags[i] = raw_data[get_dim_prob() + i] > 0.5;
    }
    
    gauss_factor = arma::det(ch)/sqrt(pow(pi, dim)*det_cov);
}

void c_rand_var_norm::dat_to_dist(const double *x) {
    partial_dat_to_dist(0, 2*dim_prob, x);
}

void c_rand_var_norm::partial_dat_to_dist(size_t lower,
                                          size_t upper,
                                          const double *x) {
    size_t k = 0;
    for (size_t i = lower; i < upper; ++i) {
        raw_data[i] = *(x+k);
        ++k;
    }
}

size_t c_rand_var_norm::get_dim() const {
    return dim;
}

size_t c_rand_var_norm::get_dim_prob() const {
    return dim_prob;
}

size_t c_rand_var_norm::get_opt_dim() const {
    size_t n = 0;
    for (size_t i = 0; i < dim_prob; ++i) {
        n += raw_data[dim_prob+i];
    }
    return n;
}
