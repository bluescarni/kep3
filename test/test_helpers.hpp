// Copyright 2023, 2024 Dario Izzo (dario.izzo@gmail.com), Francesco Biscani
// (bluescarni@gmail.com)
//
// This file is part of the kep3 library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef kep3_TEST_HELPERS_H
#define kep3_TEST_HELPERS_H

#include <algorithm>
#include <array>
#include <cmath>

#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/xadapt.hpp>

namespace kep3_tests {
using xt::linalg::cross;
using xt::linalg::dot;
// This is a float test which, while controversial, will test for abs
// differences in small numbers, relative otherwise.
inline double floating_point_error(double a, double b) {
  return std::abs(a - b) / std::max(1., std::max(a, b));
}

// This tests how close two vectors are in the euclidean metric. err = r2-r1
inline double floating_point_error_vector(const std::array<double, 3> &r1,
                                          const std::array<double, 3> &r2) {
  double R1 = std::sqrt(r1[0] * r1[0] + r1[1] * r1[1] + r1[2] * r1[2]);
  std::array<double, 3> r12 = {{r2[0] - r1[0], r2[1] - r1[1], r2[2] - r1[2]}};
  double R12 = std::sqrt(r12[0] * r12[0] + r12[1] * r12[1] + r12[2] * r12[2]);
  return R12 / std::max(1., R1);
}

// see Battin: "An Introduction to the Mathematics and Methods of
// Astrodynamics, Revised Edition", Introduction.
//
// On Keplerian dynamics the following must hold.
//
// (v1 x r1).(v1 x (r2 - r1)) + mu r2 . (r2/|r2| - r1/{|r1|})
inline double delta_guidance_error(const std::array<double, 3> &r1,
                                   const std::array<double, 3> &r2,
                                   const std::array<double, 3> &v1, double mu) {
  const auto r1_x = xt::adapt(r1);
  const auto r2_x = xt::adapt(r2);
  const auto v1_x = xt::adapt(v1);
  auto F = dot(cross(v1_x, r1_x), cross(v1_x, (r2_x - r1_x))) +
           mu * dot(r2_x, (r2_x / xt::linalg::norm(r2_x) -
                           r1_x / xt::linalg::norm(r1_x)));
  return F(0);
}
} // namespace kep3_tests

#endif // kep3_TEST_HELPERS_H
