/*
 * This file is part of QCL, a small OpenCL interface which makes it quick and
 * easy to use OpenCL.
 * 
 * Copyright (C) 2016  Aksel Alpay
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QCL_MODULE_HPP
#define QCL_MODULE_HPP

#include <string>
#include <boost/preprocessor/stringize.hpp>


/// Defines a module containing CL sources with a given name to identify it.
/// CL modules allow writing CL code without quotation marks and they do not
/// necessarily require seperate files.
/// A CL module must always be started by a call to \c cl_source_module,
/// optionally followed by one or several calls to \c include_cl_module.
/// It must always be terminated by a call to \c cl_source which specifies the
/// actual source code.
#define cl_source_module(module_name)                        \
class module_name                                            \
{                                                            \
  static std::string _concatenate_includes()                 \
  {                                                          \
    std::string result;                         

/// If your CL module needs to use functions from a different module,
/// include_cl_module will include this module and hence grant access to these
/// functions.
#define include_cl_module(include_name) result += include_name::_raw_src;

/// Specifies the source code of a CL module. \c For each call to cl_source_module,
/// there must be exactly one call to cl_source following.
#define cl_source(src)                                             \
    return result;                                                 \
  }                                                                \
public:                                                            \
  static constexpr const char* _raw_src = BOOST_PP_STRINGIZE(src); \
  static std::string source()                                      \
  {                                                                \
    return _concatenate_includes() + _raw_src;                     \
  }                                                                \
};

#endif