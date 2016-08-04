/*
 * getenv.hh
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef GETENV_HH
#define GETENV_HH

#include <string>
#include <experimental/optional>

namespace util {
    using std::experimental::optional;
    optional<std::string> getenv(const std::string&);
} // namespace util

#endif /* !GETENV_HH */
