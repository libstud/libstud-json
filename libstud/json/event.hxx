#pragma once

#include <cstddef>
#include <cstdint>

namespace stud
{
  namespace json
  {
    // Parsing/serialization event.
    //
    enum class event: std::uint8_t
    {
      begin_object = 1,
      end_object,
      begin_array,
      end_array,
      name,
      string,
      number,
      boolean,
      null
    };

    constexpr std::size_t event_count = 9;
  }
}
