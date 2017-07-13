// [[Rcpp::depends(rstan)]]
// [[Rcpp::depends(RcppEigen)]]
// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::depends(RcppArmadillo)]]


#include <RcppArmadillo.h>
#include <stan/math.hpp>
#include <vector>
#include <cmath>
#include <math.h>
#include <Eigen/Dense>
#include <RcppEigen.h>
#include <Rcpp.h>
#include <boost/math/distributions.hpp> 
#include <cstdlib> 
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace std;
using Eigen::Matrix;
using Eigen::Dynamic;
using Eigen::MatrixXd; 
using Eigen::Map;
using namespace arma;

// [[Rcpp::export]]
mat sobol_points(int N, int D) {
  ifstream infile("new-joe-kuo-6.21201" ,ios::in);
  if (!infile) {
    cout << "Input file containing direction numbers cannot be found!\n";
    exit(1);
  }
  char buffer[1000];
  infile.getline(buffer,1000,'\n');
  
  // L = max number of bits needed 
  unsigned L = (unsigned)ceil(log((double)N)/log(2.0)); 
  
  // C[i] = index from the right of the first zero bit of i
  unsigned *C = new unsigned [N];
  C[0] = 1;
  for (unsigned i=1;i<=N-1;i++) {
    C[i] = 1;
    unsigned value = i;
    while (value & 1) {
      value >>= 1;
      C[i]++;
    }
  }
  
  // POINTS[i][j] = the jth component of the ith point
  //                with i indexed from 0 to N-1 and j indexed from 0 to D-1
  mat POINTS(N, D, fill::zeros);
  
  // ----- Compute the first dimension -----
  
  // Compute direction numbers V[1] to V[L], scaled by pow(2,32)
  unsigned *V = new unsigned [L+1]; 
  for (unsigned i=1;i<=L;i++) V[i] = 1 << (32-i); // all m's = 1
  
  // Evalulate X[0] to X[N-1], scaled by pow(2,32)
  unsigned *X = new unsigned [N];
  X[0] = 0;
  for (unsigned i=1;i<=N-1;i++) {
    X[i] = X[i-1] ^ V[C[i-1]];
    POINTS(i, 0) = (double)X[i]/pow(2.0,32); // *** the actual points
    //        ^ 0 for first dimension
  }
  
  // Clean up
  delete [] V;
  delete [] X;
  
  
  // ----- Compute the remaining dimensions -----
  for (unsigned j=1;j<=D-1;j++) {
    
    // Read in parameters from file 
    unsigned d, s;
    unsigned a;
    infile >> d >> s >> a;
    unsigned *m = new unsigned [s+1];
    for (unsigned i=1;i<=s;i++) infile >> m[i];
    
    // Compute direction numbers V[1] to V[L], scaled by pow(2,32)
    unsigned *V = new unsigned [L+1];
    if (L <= s) {
      for (unsigned i=1;i<=L;i++) V[i] = m[i] << (32-i); 
    }
    else {
      for (unsigned i=1;i<=s;i++) V[i] = m[i] << (32-i); 
      for (unsigned i=s+1;i<=L;i++) {
        V[i] = V[i-s] ^ (V[i-s] >> s); 
        for (unsigned k=1;k<=s-1;k++) 
          V[i] ^= (((a >> (s-1-k)) & 1) * V[i-k]); 
      }
    }
    
    // Evalulate X[0] to X[N-1], scaled by pow(2,32)
    unsigned *X = new unsigned [N];
    X[0] = 0;
    for (unsigned i=1;i<=N-1;i++) {
      X[i] = X[i-1] ^ V[C[i-1]];
      POINTS(i, j) = (double)X[i]/pow(2.0,32); // *** the actual points
      //        ^ j for dimension (j+1)
    }
    
    // Clean up
    delete [] m;
    delete [] V;
    delete [] X;
  }
  delete [] C;
  
  return POINTS;
}

struct logQ {
  const double mu;
  const double tau;
  const double sigSq;
  logQ(const double& min, const double& tin, const double& sin) :
    mu(min), tau(tin), sigSq(sin) {}
  template <typename T>
  T operator ()(const Matrix<T, Dynamic, 1>& lambda)
    const{
      using std::log; using stan::math::lgamma; using std::pow; using std::exp;
      T qMu = -0.5 * lambda(1) - pow(mu - lambda(0), 2) / (2*exp(lambda(1)));
      T qTau = lgamma(exp(lambda(2))) + lgamma(exp(lambda(3))) - lgamma(exp(lambda(2))+exp(lambda(3))) + (exp(lambda(2))-1)*tau + (exp(lambda(3))-1)*(1-tau);
      T qSigSq = exp(lambda(4))*lambda(5) -lgamma(exp(lambda(4))) -(exp(lambda(4))+1)*log(sigSq) - exp(lambda(5))/sigSq;
      return qMu + qTau + qSigSq;
  }
};

double logP(double mu, double tau, double sigSq){
  double pMu = -pow(mu, 2) / 20;
  double pTau = 19.0 * log(tau) + 0.5 * log(1-tau);
  double pSigSq = -(2.5 + 1) * log(sigSq) - 0.025 / sigSq;
  return pMu + pTau + pSigSq;
}

// [[Rcpp::export]]
double PF(MatrixXd y, double mu, double tau, double sigSq, int N){
  int T = y.size();
  double phi = 2*tau - 1;
  // This stores xt. Begin with x0 sampled from its unconditional distribution.
  vec xOld = mu + sqrt(sigSq / (1-pow(phi, 2))) * randn<vec>(N);
  // This stores xt+1
  vec xNew(N);
  // This stores xt tilde
  vec xResample(N);
  // Weights etc.
  vec pi(N); pi.fill(1.0/N);
  vec piSum = pi;
  vec omega(N);
  // This stores logp(y|theta)
  double yAllDens = 0;
  for(int t = 0; t < T; ++t){
    // Step Zero - Take the cumulative sum of pi
    for(int k = 1; k < N; ++k){
      piSum(k) += piSum(k-1);
    }
    // Step One - Resample xt
    for(int k = 0; k < N; ++k){
      double u = randu<vec>(1)[0];
        int i = 0;
        bool flag = true;
        while(flag){
          if(u < piSum(i)){
            xResample(k) = xOld(i);
            flag = false;
          }
          if(u > piSum(N-1)){
            xResample(k) = xOld(N-1);
          }
          i += 1;
        }
      }
    // Step Two - Sample States One Step ahead and calculate omega
    for(int k = 0; k < N; ++k){
      xNew(k) = mu + phi * (xOld(k) - mu)  +  sqrt(sigSq) * randn<vec>(1)[0];
      omega(k) = 1.0 / sqrt(2*3.14159) * exp(-xNew(k)/2) * exp(-pow(y(t), 2) / (2*exp(xNew(k)))); 
    }
    // Step Three - Calculate pi
    pi = omega / sum(omega);
    double ytDens = log(sum(omega) / N);
    yAllDens += ytDens;
  }
  return yAllDens;
}

// [[Rcpp::export]]
mat shuffle(mat sobol){
  int N = sobol.n_rows;
  int D = sobol.n_cols;
  mat output(N, D, fill::zeros);
  // draw a random rule of: switch 1 and 0  /  do not switch for each binary digit.
  vec rule = randu<vec>(16);
  for(int i = 0; i < N; ++i){
    for(int j = 0; j < D; ++j){
      // grab element of the sobol sequence
      double x = sobol(i, j);
      // convert to a binary representation
      uvec binary(16, fill::zeros);
      for(int k = 1; k < 17; ++k){
        if(x > pow(2, -k)){
          binary(k-1) = 1;
          x -= pow(2, -k);
        }
      }
      // apply the transform of tilde(x_k) = x_k + a_k % 2, where a_k = 1 if rule_k > 0.5, 0 otherwise
      for(int k = 0; k < 16; ++k){
        if(rule(k) > 0.5){
          binary(k) = (binary(k) + 1) % 2;
        }
      }
      // reconstruct base 10 number from binary representation
      for(int k = 0; k < 16; ++k){
        if(binary(k) == 1){
          output(i, j) += pow(2, -(k+1));
        }
      }
    }
  }
  return output;
}

// [[Rcpp::export]]
Rcpp::List VBIL (Rcpp::NumericMatrix yIn, Rcpp::NumericMatrix lambdaIn, int S, int N, double alpha = 0.1, double threshold=0.01, int maxIter=5000){
  double beta1 = 0.9;
  double beta2 = 0.999;
  Map<MatrixXd> y(Rcpp::as<Map<MatrixXd> >(yIn));
  Map<MatrixXd> lambda(Rcpp::as<Map<MatrixXd> >(lambdaIn));
  Matrix<double, Dynamic, 1> score(6), meanGradient(6), meanGradientSq(6), Vt(6), Mt(6), LB(maxIter+1);
  
  LB.fill(0); Mt.fill(0); Vt.fill(0);
  
  
  // Initial SOBOL QMC numbers
  mat sobol = sobol_points(3, S+100).t();
  // Loop control
  int iter = 0;
  double diff = threshold + 1;
  double meanLB = 0;
  double omeanLB;
  
  while(diff > threshold){
    iter += 1;
    if(iter > maxIter){
      break;
    }
    // set up distributions to simulate theta
    boost::math::normal_distribution<> muDist(lambda(0), exp(0.5*lambda(1)));
    boost::math::beta_distribution<> tauDist(exp(lambda(2)), exp(lambda(3)));
    boost::math::inverse_gamma_distribution<> sigSqDist(exp(lambda(4)), exp(lambda(5)));
    
    meanGradient.fill(0);
    meanGradientSq.fill(0);
    // Randomly shuffle the sobol numbers for RQMC
    mat epsilon = shuffle(sobol);
    
    double logQeval;
    // Create estimates of E(dLB/dlam) and E(dLB/dlam^2)
    for(int s = 100; s < S+100; ++s){
      // generate theta
      double mu = quantile(muDist, epsilon(s, 0));
      double tau = quantile(tauDist, epsilon(s, 1));
      double sigSq = quantile(sigSqDist, epsilon(s, 2));
      // take derivative
      logQ q(mu, tau, sigSq);
      stan::math::set_zero_all_adjoints();
      stan::math::gradient(q, lambda, logQeval, score);
      double prior = logP(mu, tau, sigSq);
      double loglikelihood = PF(y,mu, tau, sigSq, N);
      // update estimates
      for(int i = 0; i < 6; ++i){
        meanGradient(i) += score(i) * (loglikelihood + prior - logQeval)/S;
        //meanGradientSq(i) += pow(score(i) * (loglikelihood + prior - logQeval), 2)/S;
        
      }
      LB(iter-1) += (loglikelihood + prior - logQeval)/S;
    }
    // adam updating rule
    for(int i = 0; i < 6; ++i){
      Mt(i) = Mt(i) * beta1  +  (1 - beta1) * meanGradient(i);
      Vt(i) = Vt(i) * beta2  +  (1 - beta2) * meanGradientSq(i);
      if(iter > 1){
        double MtHat = Mt(i) / (1 - pow(beta1, iter));
        double VtHat = Vt(i) / (1 - pow(beta2, iter));
        lambda(i) += alpha * MtHat / (sqrt(VtHat) + pow(1, -8));
      }
    }
      
    // check convergence
    if(iter > 5){
      omeanLB = meanLB;
      meanLB = 0.2 * (LB(iter-1) + LB(iter-2) + LB(iter-3) + LB(iter-4) + LB(iter-5));
      diff = std::fabs(meanLB - omeanLB);
    } 
    // report progress
    if(iter % 5 == 0){
      Rcpp::Rcout << "Iteration: " << iter << ", ELBO: " << LB(iter-1) << std::endl;
    }
  } // End while loop
  if(iter <= maxIter){
    // iter goes up by one before checking the maxIter condition, so need to use LB(iter-2)
    Rcpp::Rcout << "Converged after " << iter << " iterations at ELBO = " << LB(iter-2) << std::endl;
  } else {
    Rcpp::Rcout << "Warning, failed to converge after " << maxIter << " iterations at ELBO = " << LB(iter-2) << std::endl;
  }
  return Rcpp::List::create(Rcpp::Named("lambda") = lambda,
                            Rcpp::Named("ELBO") = LB,
                            Rcpp::Named("Iter") = iter);
}

