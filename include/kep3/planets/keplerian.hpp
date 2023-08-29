
// Copyright 2023, 2024 Dario Izzo (dario.izzo@gmail.com), Francesco Biscani
// (bluescarni@gmail.com)
//
// This file is part of the kep3 library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef kep3_UDPLA_KEPLERIAN_H
#define kep3_UDPLA_KEPLERIAN_H

#include <array>
#include <utility>

#include <kep3/detail/visibility.hpp>
#include <kep3/epoch.hpp>
#include <kep3/planet.hpp>


namespace kep3::udpla {

class kep3_DLL_PUBLIC keplerian {

  static const std::array<double, 6> default_elements;

  kep3::epoch m_ref_epoch;
  std::array<double, 6> m_elem;
  std::string m_name;
  double m_mu_central_body;
  double m_mu_self;
  double m_radius;
  double m_safe_radius;

  friend class boost::serialization::access;
  template <typename Archive> void serialize(Archive &ar, unsigned) {
    ar &m_ref_epoch;
    ar &m_elem;
    ar &m_name;
    ar &m_mu_central_body;
    ar &m_mu_self;
    ar &m_radius;
    ar &m_safe_radius;
  }

public:
  // NOTE: in here elem is a,e,i,W,w,M (Mean anomaly, not true anomaly)
  // NOTE: added_param contains mu_self, radius and safe_radius
  explicit keplerian(const epoch &ref_epoch = kep3::epoch(0),
                     const std::array<double, 6> &elem = default_elements,
                     double mu_central_body = 1., std::string name = "Unknown",
                     std::array<double, 3> added_params = {-1., -1., -1.});

  // Mandatory UDPLA methods
  static std::array<std::array<double, 3>, 2> eph(const epoch &);

  // Optional UDPLA methods
  [[nodiscard]] std::string get_name() const;
  [[nodiscard]] double get_mu_central_body() const;
  [[nodiscard]] double get_mu_self() const;
  [[nodiscard]] double get_radius() const;
  [[nodiscard]] double get_safe_radius() const;
  [[nodiscard]] std::string get_extra_info() const;

  // Other methods
  [[nodiscard]] std::string get_ref_epoch() const;
  [[nodiscard]] std::string get_elem() const;
};
} // namespace kep3::udpla
kep3_S11N_PLANET_EXPORT_KEY(kep3::udpla::keplerian);

#endif // kep3_EPOCH_H