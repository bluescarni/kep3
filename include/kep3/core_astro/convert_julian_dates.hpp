// Copyright 2023, 2024 Dario Izzo (dario.izzo@gmail.com), Francesco Biscani
// (bluescarni@gmail.com)
//
// This file is part of the kep3 library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef kep3_CONVERT_JULIAN_DATES_H
#define kep3_CONVERT_JULIAN_DATES_H

namespace kep3 {
inline double jd2mjd(double in) { return (in - 2400000.5); }
inline double jd2mjd2000(double in) { return (in - 2451544.5); }
inline double mjd2jd(double in) { return (in + 2400000.5); }
inline double mjd2mjd2000(double in) { return (in - 51544); }
inline double mjd20002jd(double in) { return (in + 2451544.5); }
inline double mjd20002mjd(double in) { return (in + 51544); }
} // namespace kep3

#endif // kep3_CONVERT_JULIAN_DATES_H
