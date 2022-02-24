// Usage: argv[0] [libFuzzer options] <corpus-directory>
//
// A corpus containing valid inputs must be provided. Starting from an empty
// corpus is not supported.

// The input format is a sequence of events in the following form:
//
// e[llllv...]
//
// e: event type (uint8_t), 0 for absent
// l: value length (uint32_t)
// v: value bytes (UTF-8 string)
//
// LLVMFuzzerTestOneInput() takes one file from the fuzz corpus as input and
// feeds the events and values it contains to the serializer one at a
// time. The file will first be passed to LLVMFuzzerCustomMutator() which will
// perform one of a number of different kinds of mutations on it, after which
// it is passed to LLVMFuzzerTestOneInput().

#include <random>
#include <cstdint>
#include <cstdlib> // abort
#include <cstring>
#include <iostream>

#include <libstud/optional.hxx>
#include <libstud/json/serializer.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace stud::json;

// Return true if a JSON event does not come with a value.
//
static bool
valueless (uint8_t e) noexcept
{
  if (e != 0) // Absent event.
  {
    switch (static_cast<event> (e))
    {
    case event::begin_object:
    case event::end_object:
    case event::begin_array:
    case event::end_array:
      {
        return true;
        break;
      }
    case event::name:
    case event::string:
    case event::number:
    case event::boolean:
    case event::null:
      {
        return false;
        break;
      }
    }
  }

  return true;
}

// Feed the events and values contained in the input buffer to the serializer.
//
extern "C" int
LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
  using stud::optional;

  // Note that libFuzzer will invoke this function once with empty input
  // before starting the fuzzing run.
  //
  if (size == 0)
    return 0;

  // Detect when we seem to be running without a corpus.
  //
  if (size < sizeof (uint32_t))
  {
    cerr << "empty corpus" << endl;
    exit (1);
  }

  string b;
  buffer_serializer s (b);

  // Extract the event count.
  //
  uint32_t en (0);
  memcpy (&en, data + size - sizeof (en), sizeof (en));

  // Parse events and their values from data and pass them to the serializer.
  //
  for (size_t ei (0), i (0); ei != en; ei++)
  {
    const uint8_t e (data[i++]);

    // Extract the value length and the value.
    //
    const char* v (nullptr);
    uint32_t n (0);
    if (!valueless (e))
    {
      memcpy (&n, data + i, sizeof (n));
      i += sizeof (n);
      v = reinterpret_cast<const char*> (data + i);
      i += n;
    }

    // Serialize the event and its value.
    //
    try
    {
      s.next (e != 0 ? static_cast<event> (e) : optional<event> (),
              {v, n},
              true /* check */);
    }
    catch (const invalid_json_output& e)
    {
      // If the error code is buffer_overflow the bug must be in the
      // serializer's code because this driver serializes to a std::string so
      // a real allocation failure would throw bad_alloc.
      //
      if (e.code == invalid_json_output::error_code::buffer_overflow)
        abort ();

      break;
    }
  }

  return 0;
}

extern "C" size_t
LLVMFuzzerMutate (uint8_t* data, size_t size, size_t maxsize);

// Default values for the event insertion mutation indexed by event.
//
static const char* default_values[event_count] {
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  "fuzz-name",
  "fuzz-string",
  "1234",
  "true",
  "null"};

// Select an event at random and either mutate its type, mutate its value
// (including updating its length), remove it, or insert a new event after it.
//
// The seed argument is a pseudo-random number which should be used in such a
// way as to cause a different mutation to be performed on each invocation.
//
// LibFuzzer's main fuzz loop (which is infinite by default) works as follows:
// an input file is selected at random. It then loops over that input a
// maximum of 5 times with each iteration consisting of a mutation (an
// invocation of LLVMFuzzerCustomMutator()) and a test (an invocation of
// LLVMFuzzerTestOneInput()). The same buffer is passed to the mutator each
// time, so mutations are cumulative. If coverage increases or the input was
// reduced, the inner loop is terminated immediately and the outer loop
// selects the next input. Note that each input will be selected for these
// 5-iteration runs repeatedly and thus ultimately be invoked many times (with
// different seed values).
//
// What this all means in the end (and according to our understanding), is
// that we don't want to perform pervasive mutations where the entire input is
// changed. Instead, we want to perform a small, localized mutation on each
// step (at least this is how the default mutation works if we did not provide
// a custom mutator).
//
// This function performs a single mutation per invocation. It selects an
// event to mutate and mutation type based on the seed argument.
//
extern "C" size_t
LLVMFuzzerCustomMutator (uint8_t* data,
                         size_t size,
                         size_t maxsize,
                         unsigned int seed)
{
  // Looking at other custom mutator implementations, it seems this is how
  // the seed should be used.
  //
  minstd_rand rand (seed);

  // Read the event count from the end of the input.
  //
  uint32_t en (0);
  memcpy (&en, data + size - sizeof (en), sizeof (en));

  // The plan is as follows: iterate over events in data copying them over to
  // the temporary buffer until we reach the event that we want to mutate.
  // Once we've performed the mutation (and added the result into the buffer),
  // we continue iterating over the remaining events copying them over into
  // the buffer as long as they fit.
  //
  uint32_t em (0);  // Number of events appended.
  vector<char> buf; // Buffer to which mutated input is written.
  buf.reserve (maxsize);

  maxsize -= sizeof (en);

  // Append a simple value to the buffer (note: assumes sufficient space).
  //
  auto append_v = [&buf] (auto v)
  {
    auto p (reinterpret_cast<const char*> (&v));
    buf.insert (buf.end (), p, p + sizeof (v));
  };

  // Append an event and its value, if any, to the buffer. Return false if
  // there wasn't enough space.
  //
  auto append_e = [&buf, &append_v, maxsize, &em]
    (uint8_t e, const void* v, uint32_t n)
  {
    const size_t cap (maxsize - buf.size ());

    if (cap < (v == nullptr ? sizeof (e) : sizeof (e) + sizeof (n) + n))
      return false;

    append_v (e);

    if (v != nullptr)
    {
      append_v (n);
      auto p (static_cast<const char*> (v));
      buf.insert (buf.end (), p, p + n);
    }

    em++;
    return true;
  };

  const uint32_t ej (rand () % en); // Index of event to mutate.
  for (size_t ei (0), i (0); ei != en; ei++)
  {
    const uint8_t e (data[i++]);

    // Extract the value length and the value.
    //
    const uint8_t* v (nullptr);
    uint32_t n (0);
    if (!valueless (e))
    {
      memcpy (&n, data + i, sizeof (n));
      i += sizeof (n);
      v = data + i;
      i += n;
    }

    // Copy over events that don't need mutation.
    //
    if (ei != ej)
    {
      if (!append_e (e, v, n)) // Did not fit.
        goto done;

      continue;
    }

    // Apply the mutation and append the result to the buffer, except if we're
    // removing the current event, in which case we do nothing.
    //
    switch (rand () % 4)
    {
    case 0: // Remove the current event.
      {
        // If this is the only event, then we fall through to add an event
        // instead.
        //
        if (en != 1)
          break;
      }
      // Fall through.
    case 1: // Insert a new event.
      {
        // Copy the current event to the buffer.
        //
        if (!append_e (e, v, n))
          goto done;

        // Insert a new event.
        //
        const uint8_t e1 (rand () % event_count + 1);
        const char* v1 (default_values[e1 - 1]);
        if (!append_e (e1, v1, v1 == nullptr ? 0 : strlen (v1)))
          goto done;

        break;
      }
    case 2: // Mutate the current event's value.
      {
        // If the event has no value, then we fall through to mutate the event
        // itself.
        //
        if (!valueless (e))
        {
          // Mutate the value, allowing it to grow by up to 100 bytes in size.
          //
          vector<uint8_t> v1 (n + 100);
          memcpy (v1.data (), v, n);
          const size_t n1 (LLVMFuzzerMutate (v1.data (), n, v1.size ()));

          if (!append_e (e, v1.data (), n1))
            goto done;

          break;
        }
      }
      // Fall through.
    case 3: // Mutate the current event (but not the value).
      {
        // If the new event doesn't need a value, then we drop the old value
        // (if any). If the new event needs a value and the old one did not
        // have any, then we use the default value as in the insert case
        // above.
        //
        const uint8_t e1 (rand () % event_count + 1);

        if (valueless (e1))
        {
          if (!append_e (e1, nullptr, 0))
            goto done;
        }
        else
        {
          if (!valueless (e))
          {
            if (!append_e (e1, v, n))
              goto done;
          }
          else
          {
            const char* v1 (default_values[e1 - 1]);
            if (!append_e (e1, v1, v1 == nullptr ? 0 : strlen (v1)))
              goto done;
          }
        }

        break;
      }
    }
  }

done:
  // Copy the mutated data and event count back into the input buffer.
  //
  append_v (em);
  memcpy (data, buf.data (), buf.size ());
  return buf.size ();
}
