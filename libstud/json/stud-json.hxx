#pragma once

#include <iosfwd>
#include <string>

#include <libstud/json/export.hxx>

namespace stud_json
{
  // Print a greeting for the specified name into the specified
  // stream. Throw std::invalid_argument if the name is empty.
  //
  LIBSTUD_JSON_SYMEXPORT void
  say_hello (std::ostream&, const std::string& name);
}
