#include <iostream>

#include <libstud/optional.hxx>
#include <libstud/json/parser.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace stud::json;

int
main ()
{
  using stud::nullopt;

  // Value in initial state.
  //
  {
    parser p ("1", "test");
    // assert (p.value ().empty ());
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);
  }

  // Peek in initial state (before any next()s): no value available except
  // through data().
  //
  {
    parser p ("1", "test");
    assert (p.peek () == event::number);
    // assert (p.value ().empty ());
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");
  }

  // Next in initial state.
  //
  {
    parser p ("1", "test");
    assert (p.next () == event::number);
    assert (p.value<int> () == 1);
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");
  }

  // Peek followed by next.
  //
  {
    parser p ("1", "test");
    assert (p.peek () == event::number);
    // assert (p.value ().empty ());
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");
    assert (p.data ().second == 1);

    assert (p.next () == event::number);
    assert (p.value<int> () == 1);
    assert (string (p.data ().first) == "1");
    assert (p.data ().second == 1);
  }

  // Next followed by peek.
  //
  {
    parser p ("[1,2]", "test");
    assert (p.next () == event::begin_array);
    assert (p.next () == event::number);
    assert (p.value<int> () == 1);

    assert (p.peek () == event::number);
    assert (p.value<int> () == 1);
  }

  // Latest value always available via data().
  //
  {
    parser p ("[1,222]", "test");
    assert (p.peek () == event::begin_array);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);

    assert (p.next () == event::begin_array);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);

    // Peeked value accessible in raw form.
    //
    assert (p.peek () == event::number);
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");
    assert (p.data ().second == 1);

    // Parsed value accessible in raw form.
    //
    assert (p.next () == event::number);
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");
    assert (p.data ().second == 1);

    // Peeked value once again accessible in raw form.
    //
    assert (p.peek () == event::number);
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "222");
    assert (p.data ().second == 3);
  }

  // After peek(), value() returns value from previous next().
  //
  {
    parser p ("[1, \"hello\", 3]", "test");
    assert (p.next () == event::begin_array);
    assert (p.next () == event::number);
    assert (p.value () == "1");

    assert (p.peek () == event::string);
    assert (p.value () == "1");
  }

  // Peek is idempotent.
  //
  {
    parser p ("[1, \"hello\"]", "test");
    assert (p.peek () == event::begin_array);
    assert (p.peek () == event::begin_array);

    assert (p.next () == event::begin_array);

    // Peek #1.
    //
    assert (p.peek () == event::number);
    // assert (p.value ().empty ());
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");

    // Peek #2.
    //
    assert (p.peek () == event::number);
    // assert (p.value ().empty ());
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "1");

    assert (p.next () == event::number);

    // Peek #1.
    //
    assert (p.peek () == event::string);
    assert (p.value () == "1");
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "hello");

    // Peek #2.
    //
    assert (p.peek () == event::string);
    assert (p.value () == "1");
    assert (p.data ().first != nullptr);
    assert (string (p.data ().first) == "hello");

    // Get to last value.
    //
    assert (p.next () == event::string);
    assert (p.next () == event::end_array);

    // Peek past last value.
    //
    assert (p.peek () == nullopt);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);

    // Get to EOF.
    //
    assert (p.next () == nullopt);

    // Peek at (past) EOF is idempotent.
    //
    assert (p.peek () == nullopt);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);
    assert (p.peek () == nullopt);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);
  }

  // Peek EOF.
  //
  {
    parser p ("1", "test");
    assert (p.next () == event::number);
    assert (p.peek () == nullopt);
    assert (p.value () == "1");
    assert (p.value<int> () == 1);
    assert (p.next () == nullopt);
    assert (p.peek () == nullopt);
  }

  // Parse at EOF.
  //
  {
    parser p ("1", "test");
    assert (p.next () == event::number);

    assert (p.next () == nullopt);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);

    assert (p.next () == nullopt);
    assert (p.data ().first == nullptr);
    assert (p.data ().second == 0);
  }

  // Beginning-to-end: parse only.
  //
  {
    parser p ("[1,2]", "test");
    assert (p.next () == event::begin_array);
    assert (p.next () == event::number);
    assert (p.value<int> () == 1);
    assert (p.next () == event::number);
    assert (p.value<int> () == 2);
    assert (p.next () == event::end_array);
    assert (p.next () == nullopt);
  }

  // Beginning-to-end: peek first.
  //
  {
    parser p ("[1,2,3]", "test");
    assert (p.peek () == event::begin_array);
    assert (p.peek () == event::begin_array);
    assert (p.next () == event::begin_array);
    assert (p.peek () == event::number); // 1
    assert (p.peek () == event::number); // 1
    assert (p.next () == event::number); // 1
    assert (p.next () == event::number); // 2
    assert (p.peek () == event::number); // 3
    assert (p.peek () == event::number); // 3
    assert (p.next () == event::number); // 3
    assert (p.peek () == event::end_array);
    assert (p.peek () == event::end_array);
    assert (p.next () == event::end_array);
    assert (p.peek () == nullopt);
    assert (p.peek () == nullopt);
    assert (p.next () == nullopt);
    assert (p.peek () == nullopt);
    assert (p.peek () == nullopt);
  }

  // Beginning-to-end: parse first.
  //
  {
    parser p ("[1,2,3]", "test");
    assert (p.next () == event::begin_array);
    assert (p.peek () == event::number); // 1
    assert (p.peek () == event::number); // 1
    assert (p.next () == event::number); // 1
    assert (p.peek () == event::number); // 2
    assert (p.peek () == event::number); // 2
    assert (p.next () == event::number); // 2
    assert (p.next () == event::number); // 3
    assert (p.peek () == event::end_array);
    assert (p.peek () == event::end_array);
    assert (p.next () == event::end_array);
    assert (p.peek () == nullopt);
    assert (p.peek () == nullopt);
    assert (p.next () == nullopt);
    assert (p.peek () == nullopt);
    assert (p.peek () == nullopt);
  }

  // Don't get caught out by empty JSON string.
  //
  {
    parser p ("[\"\", \"hello\"]", "test");
    assert (p.next () == event::begin_array);
    assert (p.next () == event::string);
    assert (p.value () == "");
    assert (p.peek () == event::string);
    assert (p.value () == "");
  }

  return 0;
}
