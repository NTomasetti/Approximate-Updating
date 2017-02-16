// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::depends(BH)]]
// [[Rcpp::plugins(cpp11)]]

#include <RcppArmadillo.h>
#include <math.h>
#include <boost/math/distributions/gamma.hpp>
using namespace Rcpp;
using namespace arma;
using namespace std;
using namespace boost::math;

//Depricated
// [[Rcpp::export]]
vec FFBScpp(vec y, vec theta, bool outputDraw = 1){
  int T = y.size();
  //subtract the mean from y
  y -= theta[1];
  //forward filter steps
  vec att(T, fill::ones); //0 - (T-1) -> x1 - xT
  vec ptt(T, fill::ones);
  vec draws(T+1); //0 - T -> x0 - xT
  double at;
  double pt = pow(theta[0], 2)*theta[3] + theta[3];
  double vt = y[0];
  att[0] = pt * vt / (pt + theta[2]);
  ptt[0] = pt - pow(pt, 2) / (pt + theta[2]);
  for(int t = 1; t < T; ++t){
    at = theta[0]*att[t-1];
    pt = pow(theta[0], 2)*ptt[t-1] + theta[3];
    vt = y[t] - at;
    att[t] = at + pt * vt / (pt + theta[2]);
    ptt[t] = pt - pow(pt, 2) / (pt + theta[2]);
  }     

  //backwards sampler steps
  double vstar;
  double fstar;
  double mstar;
  vec atT(T);
  vec ptT(T);
  draws[T] = att[T-1] + sqrt(ptt[T-1])*randn<vec>(1)[0];
  for(int t = T - 1; t > 0; --t){
    vstar = draws[t+1] - theta[0]*att[t-1];
    fstar = pow(theta[0], 2)*ptt[t-1] + theta[3];
    mstar = ptt[t-1]*theta[0];
    atT[t] = att[t-1] + mstar * vstar / fstar;
    ptT[t] = ptt[t-1] - pow(mstar, 2) / fstar;
    draws[t] = atT[t] + sqrt(ptT[t])*randn<vec>(1)[0];
  }
  atT[0] = theta[0]*draws[1] / (pow(theta[0], 2) + 1);
  ptT[0] = theta[3] / (pow(theta[0], 2) + 1);
  draws[0] = atT[0] + sqrt(ptT[0]) * randn<vec>(1)[0];
  if(outputDraw){
    return draws;
  } else {
    vec output(2*T + 2);
    for(int i = 0; i < T; ++i){
      output[i] = atT[i];
      output[T+i+1] = ptT[i];
    }
    output[T] = att[T-1];
    output[2*T + 1] = ptt[T-1];
    return output;
  }
}

// [[Rcpp::export]]
double logjoint(vec y, vec x, vec theta, double alphax = 1, double alphay = 1, double betax = 1, double betay = 1){
  double T = y.size();
  double part1 = - (alphax + (T+3)/2) * log(theta[3]) - (alphay + (T+2)/2) * log(theta[2]);
  double part2 = - betay;
  double part3 = - betax - x[0]/2;
  for(int t = 0; t < T; ++t){
    part2 -= pow(y[t] - x[t+1] - theta[1], 2)/2;
    part3 -= pow(x[t+1] - theta[0] * x[t], 2)/2;
  }
  return part1 + part2/theta[2] + part3/theta[3];
}

