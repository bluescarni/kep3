// Copyright 2023, 2024 Dario Izzo (dario.izzo@gmail.com), Francesco Biscani
// (bluescarni@gmail.com)
//
// This file is part of the kep3 library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <array>
#include <cmath>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/xadapt.hpp>

#include <kep3/exceptions.hpp>
#include <kep3/lambert_problem.hpp>

namespace kep3 {

using xt::linalg::cross;

const std::array<double, 3> lambert_problem::default_r1 = {{1.0, 0.0, 0.0}};
const std::array<double, 3> lambert_problem::default_r2 = {{0.0, 1.0, 0.0}};

/// Constructor
/** Constructs and solves a Lambert problem.
 *
 * \param[in] r1_a first cartesian position
 * \param[in] r2_a second cartesian position
 * \param[in] tof time of flight
 * \param[in] mu gravity parameter
 * \param[in] cw when true a retrograde orbit is assumed
 * \param[in] multi_revs maximum number of multirevolutions to compute
 */
lambert_problem::lambert_problem(const std::array<double, 3> &r1_a,
                                 const std::array<double, 3> &r2_a,
                                 double tof, // NOLINT
                                 double mu, bool cw, unsigned multi_revs)
    : m_r1(r1_a), m_r2(r2_a), m_tof(tof), m_mu(mu), m_has_converged(true),
      m_multi_revs(multi_revs) {
  // 0 - Sanity checks
  if (tof <= 0) {
    throw std::domain_error("lambert_problem: Time of flight is negative!");
  }
  if (mu <= 0) {
    throw std::domain_error(
        "lambert_problem: Gravity parameter is zero or negative!");
  }

  // Creating xtensor objects binded to the kep3 arrays
  const auto r1 = xt::adapt(r1_a);
  const auto r2 = xt::adapt(r2_a);

  // 1 - Getting lambda and T
  m_c = xt::linalg::norm(r2 - r1);

  double R1 = xt::linalg::norm(r1);
  double R2 = xt::linalg::norm(r2);
  m_s = (m_c + R1 + R2) / 2.0;

  auto ir1 = r1 / R1;
  auto ir2 = r2 / R2;
  auto ih = cross(ir1, ir2);
  ih = ih / xt::linalg::norm(ih);

  if (ih(2) == 0) {
    throw std::domain_error(
        "lambert_problem: The angular momentum vector has no z component, "
        "impossible to define automatically clock or "
        "counterclockwise");
  }
  double lambda2 = 1.0 - m_c / m_s;
  m_lambda = std::sqrt(lambda2);

  auto it1 = cross(ih, ir1);
  auto it2 = cross(ih, ir2);
  it1 = it1 / xt::linalg::norm(it1);
  it2 = it2 / xt::linalg::norm(it2);

  if (ih(2) < 0.0) // Transfer angle is larger than 180 degrees as seen from
                   // above the z axis
  {
    m_lambda = -m_lambda;
    it1 = -it1;
    it2 = -it2;
  }
  if (cw) { // Retrograde motion
    m_lambda = -m_lambda;
    it1 = -it1;
    it2 = -it2;
  }
  double lambda3 = m_lambda * lambda2;
  double T = std::sqrt(2.0 * m_mu / m_s / m_s / m_s) * m_tof;

  // 2 - We now have lambda, T and we will find all x
  // 2.1 - Let us first detect the maximum number of revolutions for which there
  // exists a solution
  m_Nmax = static_cast<unsigned>(T / kep3::pi);
  m_Nmax = std::min(m_multi_revs, m_Nmax);
  double T00 = std::acos(m_lambda) + m_lambda * std::sqrt(1.0 - lambda2);
  double T0 = (T00 + m_Nmax * kep3::pi);
  double T1 = 2.0 / 3.0 * (1.0 - lambda3), DT = 0.0, DDT = 0.0, DDDT = 0.0;
  if (m_Nmax > 0) {
    if (T < T0) { // We use Halley iterations to find xM and TM
      int it = 0;
      double err = 1.0;
      double T_min = T0;
      double x_old = 0.0, x_new = 0.0;
      while (true) {
        dTdx(DT, DDT, DDDT, x_old, T_min);
        if (DT != 0.0) {
          x_new = x_old - DT * DDT / (DDT * DDT - DT * DDDT / 2.0);
        }
        err = std::abs(x_old - x_new);
        if ((err < 1e-13) || (it > 12)) {
          break;
        }
        x2tof(T_min, x_new, m_Nmax);
        x_old = x_new;
        it++;
      }
      if (T_min > T) {
        m_Nmax -= 1;
      }
    }
    // We exit this if clause with Nmax being the maximum number of revolutions
    // for which there exists a solution. We crop it to m_multi_revs
    m_Nmax = std::min(m_multi_revs, m_Nmax);
  }

  // 2.2 We now allocate the memory for the output variables
  m_v1.resize(static_cast<size_t>(m_Nmax) * 2 + 1);
  m_v2.resize(static_cast<size_t>(m_Nmax) * 2 + 1);
  m_iters.resize(static_cast<size_t>(m_Nmax) * 2 + 1);
  m_x.resize(static_cast<size_t>(m_Nmax) * 2 + 1);

  // 3 - We may now find all solutions in x,y
  // 3.1 0 rev solution
  // 3.1.1 initial guess
  if (T >= T00) {
    m_x[0] = -(T - T00) / (T - T00 + 4);
  } else if (T <= T1) {
    m_x[0] = T1 * (T1 - T) / (2.0 / 5.0 * (1 - lambda2 * lambda3) * T) + 1;
  } else {
    m_x[0] =
        std::pow((T / T00), 0.69314718055994529 / std::log(T1 / T00)) - 1.0;
  }
  // 3.1.2 Householder iterations
  m_iters[0] = householder(T, m_x[0], 0, 1e-5, 15);
  // 3.2 multi rev solutions
  double tmp = 0.;
  for (std::vector<double>::size_type i = 1u; i < m_Nmax + 1; ++i) {
    // 3.2.1 left Householder iterations
    tmp = std::pow((static_cast<double>(i) * kep3::pi + kep3::pi) / (8.0 * T),
                   2.0 / 3.0);
    m_x[2 * i - 1] = (tmp - 1) / (tmp + 1);
    m_iters[2 * i - 1] =
        householder(T, m_x[2 * i - 1], static_cast<unsigned>(i), 1e-8, 15);
    // 3.2.1 right Householder iterations
    tmp = std::pow((8.0 * T) / (static_cast<double>(i) * kep3::pi), 2.0 / 3.0);
    m_x[2 * i] = (tmp - 1) / (tmp + 1);
    m_iters[2ul * i] =
        householder(T, m_x[2 * i], static_cast<unsigned>(i), 1e-8, 15);
  }

  // 4 - For each found x value we reconstruct the terminal velocities
  double gamma = std::sqrt(m_mu * m_s / 2.0);
  double rho = (R1 - R2) / m_c;
  double sigma = std::sqrt(1 - rho * rho);
  double vr1 = 0., vt1 = 0., vr2 = 0., vt2 = 0., y = 0.;
  for (size_t i = 0; i < m_x.size(); ++i) {
    y = std::sqrt(1.0 - lambda2 + lambda2 * m_x[i] * m_x[i]);
    vr1 =
        gamma * ((m_lambda * y - m_x[i]) - rho * (m_lambda * y + m_x[i])) / R1;
    vr2 =
        -gamma * ((m_lambda * y - m_x[i]) + rho * (m_lambda * y + m_x[i])) / R2;
    double vt = gamma * sigma * (y + m_lambda * m_x[i]);
    vt1 = vt / R1;
    vt2 = vt / R2;
    for (auto j = 0lu; j < 3lu; ++j) {
      m_v1[i][j] = vr1 * ir1[j] + vt1 * it1[j];
    }
    for (auto j = 0lu; j < 3lu; ++j) {
      m_v2[i][j] = vr2 * ir2[j] + vt2 * it2[j];
    }
  }
}

unsigned lambert_problem::householder(double T, double &x0,
                                      unsigned N, // NOLINT
                                      double eps, unsigned iter_max) const {
  unsigned it = 0;
  double err = 1.0;
  double xnew = 0.0;
  double tof = 0.0, delta = 0.0, DT = 0.0, DDT = 0.0, DDDT = 0.0;
  while ((err > eps) && (it < iter_max)) {
    x2tof(tof, x0, N);
    dTdx(DT, DDT, DDDT, x0, tof);
    delta = tof - T;
    double DT2 = DT * DT;
    xnew = x0 - delta * (DT2 - delta * DDT / 2.0) /
                    (DT * (DT2 - delta * DDT) + DDDT * delta * delta / 6.0);
    err = std::abs(x0 - xnew);
    x0 = xnew;
    it++;
  }
  return it;
}

void lambert_problem::dTdx(double &DT, double &DDT, double &DDDT, double x,
                           double T) const {
  double l2 = m_lambda * m_lambda;
  double l3 = l2 * m_lambda;
  double umx2 = 1.0 - x * x;
  double y = std::sqrt(1.0 - l2 * umx2);
  double y2 = y * y;
  double y3 = y2 * y;
  DT = 1.0 / umx2 * (3.0 * T * x - 2.0 + 2.0 * l3 * x / y);
  DDT = 1.0 / umx2 * (3.0 * T + 5.0 * x * DT + 2.0 * (1.0 - l2) * l3 / y3);
  DDDT = 1.0 / umx2 *
         (7.0 * x * DDT + 8.0 * DT - 6.0 * (1.0 - l2) * l2 * l3 * x / y3 / y2);
}

void lambert_problem::x2tof2(double &tof, double x, // NOLINT
                             unsigned N) const {
  double a = 1.0 / (1.0 - x * x);
  if (a > 0) // ellipse
  {
    double alfa = 2.0 * std::acos(x);
    double beta = 2.0 * std::asin(std::sqrt(m_lambda * m_lambda / a));
    if (m_lambda < 0.0) {
      beta = -beta;
    }
    tof = ((a * std::sqrt(a) *
            ((alfa - std::sin(alfa)) - (beta - std::sin(beta)) +
             2.0 * kep3::pi * N)) /
           2.0);
  } else {
    double alfa = 2.0 * std::acosh(x);
    double beta = 2.0 * std::asinh(std::sqrt(-m_lambda * m_lambda / a));
    if (m_lambda < 0.0) {
      beta = -beta;
    }
    tof = (-a * std::sqrt(-a) *
           ((beta - std::sinh(beta)) - (alfa - std::sinh(alfa))) / 2.0);
  }
}

void lambert_problem::x2tof(double &tof, double x, unsigned N) const {
  double battin = 0.01;
  double lagrange = 0.2;
  double dist = std::abs(x - 1);
  if (dist < lagrange && dist > battin) { // We use Lagrange tof expression
    x2tof2(tof, x, N);
    return;
  }
  double K = m_lambda * m_lambda;
  double E = x * x - 1.0;
  double rho = std::abs(E);
  double z = std::sqrt(1 + K * E);
  if (dist < battin) { // We use Battin series tof expression
    double eta = z - m_lambda * x;
    double S1 = 0.5 * (1.0 - m_lambda - x * eta);
    double Q = hypergeometricF(S1, 1e-11);
    Q = 4.0 / 3.0 * Q;
    tof = (eta * eta * eta * Q + 4.0 * m_lambda * eta) / 2.0 +
          N * kep3::pi / std::pow(rho, 1.5);
    return;
  } else { // We use Lancaster tof expresion
    double y = std::sqrt(rho);
    double g = x * z - m_lambda * E;
    double d = 0.0;
    if (E < 0) {
      double l = std::acos(g);
      d = N * kep3::pi + l;
    } else {
      double f = y * (z - m_lambda * x);
      d = std::log(f + g);
    }
    tof = (x - m_lambda * z - d / y) / E;
    return;
  }
}

double lambert_problem::hypergeometricF(double z, double tol) const { // NOLINT
  double Sj = 1.0;
  double Cj = 1.0;
  double err = 1.0;
  double Cj1 = 0.0;
  double Sj1 = 0.0;
  int j = 0;
  while (err > tol) {
    Cj1 = Cj * (3.0 + j) * (1.0 + j) / (2.5 + j) * z / (j + 1);
    Sj1 = Sj + Cj1;
    err = std::abs(Cj1);
    Sj = Sj1;
    Cj = Cj1;
    j = j + 1;
  }
  return Sj;
}

/// Gets velocity at r1
/**
 *
 * \return an std::vector containing 3-d arrays with the cartesian components of
 * the velocities at r1 for all 2N_max+1 solutions
 */
const std::vector<std::array<double, 3>> &lambert_problem::get_v1() const {
  return m_v1;
}

/// Gets velocity at r2
/**
 *
 * \return an std::vector containing 3-d arrays with the cartesian components of
 * the velocities at r2 for all 2N_max+1 solutions
 */
const std::vector<std::array<double, 3>> &lambert_problem::get_v2() const {
  return m_v2;
}

/// Gets r1
/**
 *
 * \return a 3-d array with the cartesian components of r1
 */
const std::array<double, 3> &lambert_problem::get_r1() const { return m_r1; }

/// Gets r2
/**
 *
 * \return a 3-d array with the cartesian components of r2
 */
const std::array<double, 3> &lambert_problem::get_r2() const { return m_r2; }

/// Gets the time of flight between r1 and r2
/**
 *
 * \return the time of flight
 */
const double &lambert_problem::get_tof() const { return m_tof; }

/// Gets the x variable
/**
 * Gets the x variable for each solution found (0 revs, 1,1,2,2,3,3 .... N,N)
 *
 * \return the x variables in an std::vector
 */
const std::vector<double> &lambert_problem::get_x() const { return m_x; }

/// Gets gravitational parameter
/**
 *
 * \return the gravitational parameter
 */
const double &lambert_problem::get_mu() const { return m_mu; }

/// Gets number of iterations
/**
 *
 * \return an std::vector containing the iterations taken to compute each one of
 * the solutions
 */
const std::vector<unsigned> &lambert_problem::get_iters() const {
  return m_iters;
}

/// Gets N_max
/**
 *
 * \return the maximum number of revolutions. The number of solutions to the
 * problem will be Nmax*2 +1
 */
unsigned lambert_problem::get_Nmax() const { return m_Nmax; }

/// Streaming operator
std::ostream &operator<<(std::ostream &s, const lambert_problem &lp) {
  s << std::setprecision(16) << "Lambert's problem:" << std::endl;
  s << "mu = " << lp.m_mu << std::endl;
  s << "r1 = "
    << "[" << lp.m_r1[0] << ", " << lp.m_r1[1] << ", " << lp.m_r1[2] << "]"
    << std::endl;
  s << "r2 = "
    << "[" << lp.m_r2[0] << ", " << lp.m_r2[1] << ", " << lp.m_r2[2] << "]"
    << std::endl;
  s << "Time of flight: " << lp.m_tof << std::endl << std::endl;
  s << "chord = " << lp.m_c << std::endl;
  s << "semiperimeter = " << lp.m_s << std::endl;
  s << "lambda = " << lp.m_lambda << std::endl;
  s << "non dimensional time of flight = "
    << lp.m_tof * sqrt(2 * lp.m_mu / lp.m_s / lp.m_s / lp.m_s) << std::endl
    << std::endl;
  s << "Maximum number of revolutions: " << lp.m_Nmax << std::endl;
  s << "Solutions: " << std::endl;
  s << "0 revs, Iters: " << lp.m_iters[0] << ", x: " << lp.m_x[0]
    << ", a: " << lp.m_s / 2.0 / (1 - lp.m_x[0] * lp.m_x[0]) << std::endl;
  s << "\tv1 = "
    << "[" << lp.m_v1[0][0] << ", " << lp.m_v1[0][1] << ", " << lp.m_v1[0][2]
    << "]";
  s << " v2 = "
    << "[" << lp.m_v2[0][0] << ", " << lp.m_v2[0][1] << ", " << lp.m_v2[0][2]
    << "]" << std::endl;
  for (std::vector<double>::size_type i = 0lu; i < lp.m_Nmax; ++i) {
    s << i + 1 << " revs,  left. Iters: " << lp.m_iters[1 + 2 * i]
      << ", x: " << lp.m_x[1 + 2 * i]
      << ", a: " << lp.m_s / 2.0 / (1 - lp.m_x[1 + 2 * i] * lp.m_x[1 + 2 * i])
      << std::endl;
    s << "\tv1 = "
      << "[" << lp.m_v1[1 + 2 * i][0] << ", " << lp.m_v1[1 + 2 * i][1] << ", "
      << lp.m_v1[1 + 2 * i][2] << "]";
    s << " v2 = "
      << "[" << lp.m_v2[1 + 2 * i][0] << ", " << lp.m_v2[1 + 2 * i][1] << ", "
      << lp.m_v2[1 + 2 * i][2] << "]" << std::endl;
    s << i + 1 << " revs, right. Iters: " << lp.m_iters[2 + 2 * i]
      << ", a: " << lp.m_x[2 + 2 * i]
      << ", a: " << lp.m_s / 2.0 / (1 - lp.m_x[2 + 2 * i] * lp.m_x[2 + 2 * i])
      << std::endl;
    s << "\tv1 = "
      << "[" << lp.m_v1[2 + 2 * i][0] << ", " << lp.m_v1[2 + 2 * i][1] << ", "
      << lp.m_v1[2 + 2 * i][2] << "]";
    s << " v2 = "
      << "[" << lp.m_v2[2 + 2 * i][0] << ", " << lp.m_v2[2 + 2 * i][1] << ", "
      << lp.m_v2[2 + 2 * i][2] << "]" << std::endl;
  }
  return s;
}

} // namespace kep3