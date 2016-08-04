/*
 * getenv.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "getenv.hh"
#include <cstdlib>

namespace util {

optional<std::string> getenv(const std::string& name)
{
    auto value = ::getenv(name.c_str());
    return value ? optional<std::string>(value) : optional<std::string>();
}

} // namespace util
