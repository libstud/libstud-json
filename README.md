# libstud-json - JSON parser/serializer library for C++

A portable, dependency-free, MIT-licensed JSON pull-parser/push-serializer
library for C++.

The goal of this library is to provide a *pull*-style parser (instead of
*push*/SAX or DOM) and *push*-style serializer with clean, modern interfaces
and conforming, well-tested (and well-fuzzed, including the serializer)
implementations. In particular, pull-style parsers are not very common, and we
couldn't find any C++ implementations that also satisfy the above
requirements.

Typical parser usage (low-level API):

```c++
#include <libstud/json/parser.hxx>

int main ()
{
  using namespace stud::json;

  parser p (std::cin, "<stdin>");

  for (event e: p)
  {
    switch (e)
    {
    case event::begin_object:
      // ...
    case event::end_object:
      // ...
    case event::name:
      {
        const std::string& n (p.name ());
        // ...
      }
    case event::number:
      {
        std::int64_t n (p.value<std::int64_t> ());
        // ...
      }
    }
  }
}
```

Or using the higher-level API to parse a specific JSON vocabulary:

```c++
#include <libstud/json/parser.hxx>

int main ()
{
  using namespace stud::json;

  parser p (std::cin, "<stdin>");

  p.next_expect (event::begin_object);
  {
    std::string planet (p.next_expect_member_string ("planet"));

    p.next_expect_member_array ("measurements");
    while (p.next_expect (event::number, event::end_array))
    {
      std::uint64 m (p.value<std::uint64> ());
    }
  }
  p.next_expect (event::end_object);
}
```

See the [`libstud/json/parser.hxx`][parser.hxx] header for the parser
interface details and the [`libstud/json/event.hxx`][event.hxx] header for the
complete list of events.

Typical serializer usage:

```c++
#include <libstud/json/serializer.hxx>

int main ()
{
  using namespace stud::json;

  stream_serializer s (std::cout);

  s.begin_object ();
  s.member ("planet", "Venus");
  s.member_name ("measurement");
  s.begin_array ();
  s.value (123);
  s.value (234);
  s.value (345);
  s.end_array ();
  s.end_object ();
}
```

See the [`libstud/json/serializer.hxx`][serializer.hxx] header for the
serializer interface details.

See the [`NEWS`][news] file for changes and the
[`cppget.org/libstud-json`][pkg] package page for build status.

[event.hxx]:      https://github.com/libstud/libstud-json/blob/master/libstud/json/event.hxx
[parser.hxx]:     https://github.com/libstud/libstud-json/blob/master/libstud/json/parser.hxx
[serializer.hxx]: https://github.com/libstud/libstud-json/blob/master/libstud/json/serializer.hxx
[news]:           https://github.com/libstud/libstud-json/blob/master/NEWS
[pkg]:            https://cppget.org/libstud-json
