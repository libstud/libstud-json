// Usage: argv[0] [--check] [--pretty]
//
// --check   -- enable UTF-8 checking and escaping
// --pretty  -- enable pretty-printing

#include <iostream>

#include <libstud/optional.hxx>
#include <libstud/json/parser.hxx>
#include <libstud/json/serializer.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace stud::json;

int
main (int argc, const char* argv[])
{
  using stud::nullopt;

  bool check (false);
  bool pretty (false);

  for (int i (1); i < argc; i++)
  {
    const string o (argv[i]);

    if (o == "--check")
      check = true;
    else if (o == "--pretty")
      pretty = true;
  }

  parser p (cin, "<stdin>", true /* multi_value*/);
  stream_serializer s (cout, pretty ? 2 : 0);

  try
  {
    if (p.peek ())
    {
      while (p.peek ())
      {
        for (event e: p)
          s.next (e, p.data (), check);
        s.next (nullopt);
      }
      s.next (nullopt);
      cout << endl;
    }

    return 0;
  }
  catch (const invalid_json_output& e)
  {
    cerr << e.what () << endl;
  }
  catch (const invalid_json_input& e)
  {
    cerr << e.what () << endl;
  }
  catch (const ios::failure& e)
  {
    cerr << "io error: " << e.what () << endl;
  }

  return 1;
}
