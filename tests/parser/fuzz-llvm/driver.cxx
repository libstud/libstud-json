#include <cstdint>

#include <libstud/json/parser.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace stud::json;

// Parse the data in the specified mode (default or multi-value) returning
// true if the data is valid JSON and false otherwise.
//
static bool
parse (const void* data, size_t size,
       language lang,
       bool multi,
       const char* sep = nullptr)
{
  parser p (data, size, "<buffer>", lang, multi, sep);

  auto handle_event = [&p, lang] (event e)
  {
    // Implied begin_object in JSON5E may have 0 line/column/position.
    //
    if (lang != language::json5e)
    {
      assert (p.line () >= 1 && p.column () >= 1 && p.position () >= 1);
    }

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
  // Parse the input in every mode.
  //
  // While it may see that if the input is valid in the stricter mode, then
  // parsing it in the more relaxed one would be a waste of time. However,
  // different modes may apply different parsing logic to the same input
  // (implied object handling in JSON5E is a good example).
  //
  // Note that for multi-value mode we configure the parser to accept zero or
  // more JSON whitespaces between values. The longer the list of accepted
  // separator characters, the better the balance with the hundreds of invalid
  // possibilities.
  //
  parse (data, size, language::json,   false);
  parse (data, size, language::json,   true);
  parse (data, size, language::json5,  false);
  parse (data, size, language::json5,  true);
  parse (data, size, language::json5e, false);
  parse (data, size, language::json5e, true);

  return 0;
}
