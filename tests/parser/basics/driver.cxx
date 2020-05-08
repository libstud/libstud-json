// Usage: argv[0] [--multi[=<sep>]] [--peek] --fail-exc|--fail-bit|[<mode>]
//
// --multi=<sep> -- enable multi-value mode with the specified separators
// --peek        -- pre-peek every token before parsing (must come first)
// --fail-exc    -- fail due to istream exception
// --fail-bit    -- fail due to istream badbit
// <mode>        -- numeric value parsing mode: i|u|f|d|l|

#include <cassert>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include <libstud/json/parser.hxx>

using namespace std;
namespace json = stud::json;

static string
number (const string& m, json::parser& p)
{
  if (m ==  "") return p.value ();
  if (m == "i") return to_string (p.value<int32_t> ());
  if (m == "u") return to_string (p.value<uint32_t> ());
  if (m == "f") return to_string (p.value<float> ());
  if (m == "d") return to_string (p.value<double> ());
  if (m == "l") return to_string (p.value<long double> ());

  assert (false);
  return "";
}

int main (int argc, const char* argv[])
{
  bool multi (false);
  const char* sep (nullptr);
  bool peek (false);
  bool fail_exc (false);
  bool fail_bit (false);

  string nm;
  for (int i (1); i < argc; ++i)
  {
    string o (argv[i]);

    if (o.compare (0, 7, "--multi") == 0)
    {
      multi = true;
      if (o.size () > 7)
        sep = argv[i] + 8;
      continue;
    }

    if (o == "--peek")
    {
      peek = true;
      continue;
    }

    if      (o == "--fail-exc") fail_exc = true;
    else if (o == "--fail-bit") fail_bit = true;
    else nm = move (o);
    break; // One of these should be last.
  }

  try
  {
    using namespace json;

    // It's not easy to cause the stream to fail when called by the parser.
    // So we will fail on EOF as the next best thing.
    //
    if (!fail_bit)
      cin.exceptions (istream::badbit  |
                      istream::failbit |
                      (fail_exc ? istream::eofbit : istream::goodbit));

    parser p (cin, "<stdin>", multi, sep);
    size_t i (0); // Indentation.

    cout << right << setfill (' '); // Line number formatting.

    auto process_event = [&p, &i, nm, fail_bit] (event e)
    {
      size_t j (i);
      string s;

      switch (e)
      {
      case event::begin_object: s = "{";     i += 2;                     break;
      case event::end_object:   s = "}"; j = i -= 2;                     break;
      case event::begin_array:  s = "[";     i += 2;                     break;
      case event::end_array:    s = "]"; j = i -= 2;                     break;
      case event::name:         s = p.name ();                           break;
      case event::string:       s = '"' + p.value () + '"';              break;
      case event::number:       s = number (nm, p);                      break;
      case event::boolean:      s = p.value<bool> () ? "true" : "false"; break;
      case event::null:         s = "NULL";                              break;
      }

      cout << setw (3) << p.line () << "," << setw (3) << p.column () << ": "
           << string (j, ' ') << s << '\n';

      if (fail_bit)
        cin.setstate (istream::badbit);
    };

    // Use the "canonical" parsing code for both modes.
    //
    if (!multi)
    {
      if (peek)
        p.peek ();

      for (event e: p)
      {
        process_event (e);

        if (peek)
          p.peek ();
      }
    }
    else
    {
      while (p.peek ())
        for (event e: p)
        {
          process_event (e);

          if (peek)
            p.peek ();
        }
    }

    return 0;
  }
  catch (const json::invalid_json_input& e)
  {
    cerr << e.name << ':' << e.line << ':' << e.column << ": error: "
         << e.what () << endl;
  }
  catch (const istream::failure&)
  {
    cerr << "error: unable to read from stdin" << endl;
  }

  return 1;
}
