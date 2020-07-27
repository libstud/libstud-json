# libstud-json - JSON parser/serializer library for C++

A portable, dependency-free, MIT-licensed JSON pull-parser/push-serializer
library for C++.

Typical parser usage:

```c++
#include <iostream>

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
        int n (p.value<int> ());
        // ...
      }
    }
  }
}
```

See the [`libstud/json/parser.hxx`][parser.hxx] header for the parser
interface details and the [`libstud/json/event.hxx`][event.hxx] header for the
complete list of events.

Typical serializer usage:

```c++
#include <iostream>

#include <libstud/json/serializer.hxx>

int main ()
{
  using namespace stud::json;

  stream_serializer s (std::cout);

  s.begin_object ();
  s.member ("string", "abc");
  s.member_name ("array");
  s.begin_array ();
  s.value (123);
  s.value (true);
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
