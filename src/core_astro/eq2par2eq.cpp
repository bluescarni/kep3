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

#include <boost/math/constants/constants.hpp>

namespace kep3 {

constexpr double half_pi{boost::math::constants::half_pi<double>()};

std::array<double, 6> eq2par(const std::array<double, 6> &eq,
                             bool retrogade = false) {
  std::array<double, 6> retval{};
  int I = 1;
  if (retrogade) {
    I = -1;
  }
  auto ecc = std::sqrt(eq[1] * eq[1] + eq[2] * eq[2]);
  auto tmp = std::sqrt(eq[3] * eq[3] + eq[4] * eq[4]);
  auto zita = std::atan2(eq[2] / ecc, eq[1] / ecc);

  retval[1] = ecc;
  retval[0] = eq[0] / (1. - ecc * ecc);
  retval[2] = half_pi * (1. - I) +
              2. * I * std::atan(tmp);
  retval[3] = std::atan2(eq[4] / tmp, eq[3] / tmp);
  retval[4] = zita - I * retval[3];
  retval[5] = eq[5] - zita;
  return retval;
}

std::array<double, 6> par2eq(const std::array<double, 6> &par,
                             bool retrogade = false) {
  std::array<double, 6> eq{};
  int I = 0;
  if (retrogade) {
    I = -1;
    eq[3] = 1. / std::tan(par[2] / 2) * std::cos(par[3]);
    eq[4] = 1. / std::tan(par[2] / 2) * std::sin(par[3]);
  } else {
    I = 1;
    eq[3] = std::tan(par[2] / 2) * std::cos(par[3]);
    eq[4] = std::tan(par[2] / 2) * std::sin(par[3]);
  }
  eq[0] = par[0] * (1 - par[1] * par[1]);
  eq[1] = par[1] * std::cos(par[4] + I * par[3]);
  eq[2] = par[1] * std::sin(par[4] + I * par[3]);
  eq[5] = par[5] + par[4] + I * par[3];
  return eq;
}

} // namespace kep3