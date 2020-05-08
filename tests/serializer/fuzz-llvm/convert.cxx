// This utility can be used to create an initial serializer fuzz corpus from
// valid JSON inputs. Usage:
//
// convert input.json output.bin
//
// See driver.cxx for the output format description.
//

#include <fstream>
#include <iostream>

#include <libstud/json/parser.hxx>

using namespace std;
using namespace stud::json;

int
main (int argc, const char** argv)
{
  if (argc != 3)
  {
    cerr << "usage: " << argv[0] << " <input-file> <output-file>" << endl;
    return 1;
  }

  // Setup input.
  //
  ifstream in (argv[1]);
  if (in.fail ())
  {
    cerr << "unable to open file '" << argv[1] << "' for reading" << endl;
    return 1;
  }
  in.exceptions (ios::badbit | ios::failbit);
  parser p (in, argv[1], true /* multi_value */);

  // Setup output.
  //
  ofstream out (argv[2], ios::binary);
  if (out.fail ())
  {
    cerr << "unable to open file '" << argv[2] << "' for writing" << endl;
    return 1;
  }
  out.exceptions (ios::badbit | ios::failbit);

  // Writes an event and (potentially absent, empty) value to stdout.
  //
  auto write = [&out] (uint8_t e, const string* v)
  {
    out.write (reinterpret_cast<const char*> (&e), sizeof (e));

    if (v != nullptr)
    {
      const uint32_t s (v->size ());
      auto sp (reinterpret_cast<const char*> (&s));
      out.write (sp, sizeof (s));
      out.write (v->data (), v->size ());
    }
  };

  try
  {
    uint32_t n (0); // Number of events.

    while (p.peek ())
    {
      for (event e: p)
      {
        switch (e)
        {
        case event::name:
          {
            write (static_cast<uint8_t> (e), &p.name ());
            break;
          }
        case event::string:
        case event::number:
        case event::boolean:
        case event::null:
          {
            write (static_cast<uint8_t> (e), &p.value ());
            break;
          }
        default:
          {
            write (static_cast<uint8_t> (e), nullptr);
            break;
          }
        }
        n++;
      }
      write (0, nullptr); // Absent event.
      n++;
    }
    write (0, nullptr); // Absent event.
    n++;

    out.write (reinterpret_cast<const char*> (&n), sizeof (n));
  }
  catch (const invalid_json_input& e)
  {
    cerr << e.name << ':' << e.line << ':' << e.column << ": error: "
         << e.what () << endl;
    return 1;
  }
  catch (const std::exception& e)
  {
    cerr << e.what () << endl;
    return 1;
  }
}
