#include <cassert>
#include <cstdint>

#include <libstud/json/parser.hxx>

using namespace std;
using namespace stud::json;

// Parse the data in the specified mode (default or multi-value) returning
// true if the data is valid JSON and false otherwise.
//
static bool
parse (const void* data, size_t size, bool multi, const char* sep)
{
  parser p (data, size, "<buffer>", multi, sep);

  auto handle_event = [&p] (event e)
  {
    assert (p.line () >= 1 && p.column () >= 1 && p.position () >= 1);

    switch (e)
    {
    case event::begin_object:
    case event::end_object:
    case event::begin_array:
    case event::end_array: break;
    case event::string: p.value (); break;
    case event::name: p.name (); break;
    case event::null: assert (p.value () == "null"); break;
    case event::boolean:
      {
        p.value<bool> ();
        assert (p.value () == "true" || p.value () == "false");
        break;
      }
    case event::number:
      {
        try
        {
          p.value<int64_t> ();
        }
        catch (const invalid_json_input&)
        {
          try
          {
            p.value<double> ();
          }
          catch (const invalid_json_input&)
          {
            p.value ();
          }
        }
        break;
      }
    }
  };

  try
  {
    if (!multi)
      for (auto e: p) handle_event (e);
    else
      while (p.peek ())
        for (auto e: p) handle_event (e);

    return true;
  }
  catch (const invalid_json_input&)
  {
    return false;
  }
}

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
  // If it's valid in default mode, don't waste time parsing it in multi-value
  // mode.
  //
  if (!parse (data, size, false, nullptr))
  {
    // Multi-value mode enabled and configured to accept zero or more JSON
    // whitespaces between values. The longer the list of accepted separator
    // characters, the better the balance with the hundreds of invalid
    // possibilities.
    //
    parse (data, size, true, nullptr);
  }
  return 0;
}
