// Copyright 2023, 2024 Dario Izzo (dario.izzo@gmail.com), Francesco Biscani
// (bluescarni@gmail.com)
//
// This file is part of the kep3 library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <string>

#if defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))

// GCC demangle. This is available also for clang, both with libstdc++ and
// libc++.
#include <cstdlib>
#include <cxxabi.h>
#include <memory>

#endif

#include <kep3/detail/type_name.hpp>

namespace kep3::detail {

std::string demangle_from_typeid(const char *s) {
#if defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))
  // NOTE: wrap std::free() in a local lambda, so we avoid
  // potential ambiguities when taking the address of std::free().
  // See:
  // https://stackoverflow.com/questions/27440953/stdunique-ptr-for-c-functions-that-need-free
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory, hicpp-no-malloc, cppcoreguidelines-no-malloc)
  auto deleter = [](void *ptr) { std::free(ptr); };

  // NOTE: abi::__cxa_demangle will return a pointer allocated by std::malloc,
  // which we will delete via std::free().
  std::unique_ptr<char, decltype(deleter)> res{
      ::abi::__cxa_demangle(s, nullptr, nullptr, nullptr), deleter};

  // NOTE: return the original string if demangling fails.
  return res ? std::string(res.get()) : std::string(s);
#else
  // If no demangling is available, just return the mangled name.
  // NOTE: MSVC already returns the demangled name from typeid.
  return std::string(s);
#endif
}

} // namespace kep3::detail
