#include <limits>
#include <cassert>
#include <cstring> // memcmp()
#include <sstream>

#include <libstud/json/serializer.hxx>

using namespace std;
using namespace stud::json;

// Overflow function which always grows the buffer by exactly N bytes.
//
template <size_t N>
static void
overflow (void*, event, buffer_serializer::buffer& b, size_t)
{
  b.capacity = b.size + N;
}

int
main ()
{
  using error = invalid_json_output::error_code;

  // Return true if a call to s.next () with these arguments throws an
  // invalid_json_output exception with the specified error code (ec).
  //
  auto next_throws = [] (error ec,
                         buffer_serializer& s,
                         std::optional<event> e,
                         std::pair<const char*, std::size_t> val = {},
                         bool check = true)
  {
    try
    {
      s.next (e, val, check);
      return false;
    }
    catch (const invalid_json_output& e)
    {
      return e.code == ec;
    }
  };

  // Return true if the serialization (with checking enabled) of a string
  // throws.
  //
  auto serialize_throws = [next_throws] (const string& v)
  {
    string b;
    buffer_serializer s (b);
    return next_throws (
        error::invalid_value, s, event::string, {v.c_str (), v.size ()}, true);
  };

  // Return the serialized form of a string (with checking enabled). Note that
  // the quotes are removed to ease comparisons.
  //
  auto serialize = [] (const string& v)
  {
    string b;
    buffer_serializer s (b);
    s.next (event::string, {v.c_str (), v.size ()}, true);
    return b.size () >= 2 ? b.substr (1, b.size () - 2) : "";
  };

  // Completeness of top-level JSON value sequences.
  //
  {
    // Open array detected as incomplete.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_array);
      assert (next_throws (error::invalid_value, s, nullopt));
    }

    // Open object detected as incomplete.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_object);
      assert (next_throws (error::invalid_value, s, nullopt));
    }

    // Declare top-level value sequence complete by serializing an absent
    // event (nullopt).
    //
    // After that, serializing anything, even nullopt, is an error.
    //
    {
      // Empty top-level value sequence.
      //
      // If no values have been serialized, the first absent event declares
      // the top-level value sequence complete.
      //
      {
        string b;
        buffer_serializer s (b);
        s.next (nullopt); // Declare this an empty sequence of top-level values.
        assert (next_throws (error::invalid_value, s, event::number, {"2", 1}));
        assert (next_throws (error::invalid_value, s, nullopt));
      }

      // One top-level value.
      //
      {
        string b;
        buffer_serializer s (b);
        s.next (event::number, {"1", 1});
        s.next (nullopt); // Check for completeness (throws if not).
        s.next (nullopt); // Declare end of top-level value sequence.
        assert (next_throws (error::invalid_value, s, event::number, {"2", 1}));
        assert (next_throws (error::invalid_value, s, nullopt));
      }

      // Multiple top-level values.
      //
      {
        string b;
        buffer_serializer s (b);
        s.next (event::number, {"1", 1});
        s.next (event::number, {"2", 1});
        s.next (nullopt); // Check for completeness (throws if not).
        s.next (nullopt); // Declare end of top-level value sequence.
        assert (next_throws (error::invalid_value, s, event::number, {"3", 1}));
        assert (next_throws (error::invalid_value, s, nullopt));
      }
    }
  }

  // Array structure.
  //
  {
    // End array outside array.
    //
    {
      string b;
      buffer_serializer s (b);
      assert (next_throws (error::unexpected_event, s, event::end_array));
    }

    // End object inside array.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_array);
      assert (next_throws (error::unexpected_event, s, event::end_object));
    }
  }

  // Object structure.
  //
  {
    // End object outside object.
    //
    {
      string b;
      buffer_serializer s (b);
      assert (next_throws (error::unexpected_event, s, event::end_object));
    }

    // End object when member value is expected.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_object);
      s.next (event::name, {"n", 1});
      assert (next_throws (error::unexpected_event, s, event::end_object));
    }

    // End array inside object.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_object);
      assert (next_throws (error::unexpected_event, s, event::end_array));
    }

    // Value when expecting a name.
    //
    {
      {
        string b;
        buffer_serializer s (b);
        s.next (event::begin_object);
        assert (
            next_throws (error::unexpected_event, s, event::number, {"1", 1}));
      }
      {
        string b;
        buffer_serializer s (b);
        s.next (event::begin_object);
        assert (
            next_throws (error::unexpected_event, s, event::string, {"1", 1}));
      }
      {
        string b;
        buffer_serializer s (b);
        s.next (event::begin_object);
        assert (next_throws (
            error::unexpected_event, s, event::boolean, {"true", 4}));
      }
      {
        string b;
        buffer_serializer s (b);
        s.next (event::begin_object);
        assert (
            next_throws (error::unexpected_event, s, event::null, {"null", 4}));
      }

      // When there is already a complete member.
      //
      {
        string b;
        buffer_serializer s (b);
        s.next (event::begin_object);
        s.next (event::name, {"a", 1});
        s.next (event::number, {"1", 1});
        assert (
            next_throws (error::unexpected_event, s, event::number, {"1", 1}));
      }
    }

    // Begin object when expecting a name.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_object);
      assert (next_throws (error::unexpected_event, s, event::begin_object));
    }

    // Name when expecting a value.
    //
    {
      string b;
      buffer_serializer s (b);
      s.next (event::begin_object);
      s.next (event::name, {"a", 1});
      assert (next_throws (error::unexpected_event, s, event::name, {"b", 1}));
    }
  }

  // Buffer management.
  //
  {
    // Fixed-size buffer: capacity exceeded.
    //
    {
      uint8_t b[3];
      size_t n (0);
      buffer_serializer s (b, n, 3);
      s.next (event::number, {"12", 2}); // 3 bytes written (val + newline).
      assert (next_throws (error::buffer_overflow, s, event::number, {"2", 1}));
    }

    // Serialization of value with multiple calls to overflow.
    //
    {
      uint8_t b[100];
      size_t n (0);
      buffer_serializer s (b, n, 0, &overflow<6>, nullptr, nullptr);
      const string v (50, 'a');
      s.next (event::string, {v.c_str (), v.size ()});
      // +1 skips the opening quote.
      //
      assert (memcmp (b + 1, v.c_str (), v.size ()) == 0);
    }

    // Serializer appends to user buffer (that is, preserves its contents).
    //
    {
      // String.
      //
      {
        string b ("aaa");
        buffer_serializer s (b);
        const string v ("bbb");
        s.next (event::string, {v.c_str(), v.size ()});
        assert (b == "aaa\"bbb\"");
      }

      // Array.
      //
      {
        uint8_t b[100] {'a', 'a', 'a'};
        size_t n (3);
        buffer_serializer s (b, n, 10, nullptr, nullptr, nullptr);
        const string v ("bbb");
        s.next (event::string, {v.c_str(), v.size ()});
        assert (n == 8);
        assert (memcmp (b, "aaa\"bbb\"", 8) == 0);
      }
    }

    // Regression tests.
    //
    {
      // This is a regression test for two different but related
      // buffer-management bugs.
      //
      // Whether or not either of these bugs are triggered depends on the
      // capacity of the buffer and thus on the allocation patterns of
      // std::string, and therefore it's not practical to construct a small
      // number of minimal and specific test cases. For both libstdc++ and
      // libc++, however, both bugs were triggered in under 20 characters so
      // the 100 used here should cover most implementations. (I think the
      // crucial value is the size of the SSO buffer.)
      //
      {
        for (size_t i (1); i < 100; i++)
          serialize (string (i, 'a') + "\x01");
      }

      // With this setup and input we get to the first byte of the UTF-8
      // sequence with the bytes left to be written (size, value 2) is less
      // than the bytes left in the buffer (cap, value 3) (see
      // serializer::write()). Thus a value of (size - cap = 2 - 3 =
      // underflow) was being passed to the overflow function. See the fake
      // overflow implementation above for details.
      //
      {
        uint8_t b[20];
        size_t n (0);
        buffer_serializer s (b, n, 0, &overflow<6>, nullptr, nullptr);
        // 0xF0 indicates the beginning of a 4-byte UTF-8 sequence.
        //
        const string v ("12\xF0");
        try
        {
          s.next (event::string, {v.c_str (), v.size ()}, true);
        }
        catch (const invalid_json_output& e)
        {
          assert (e.code == error::invalid_value);
        }
      }
    }
  }

  // Validation of literal values (null and boolean). All JSON literals must
  // be lower case.
  //
  {
    string b;
    buffer_serializer s (b);

    auto next_throws_invalid_value =
    [&next_throws, &s] (event e, pair<const char*, size_t> v)
    {
      return next_throws (error::invalid_value, s, e, v, true);
    };

    assert (next_throws_invalid_value (event::null, {"Null", 4}));
    assert (next_throws_invalid_value (event::null, {"NULL", 4}));
    assert (next_throws_invalid_value (event::null, {"nul", 3}));
    assert (next_throws_invalid_value (event::null, {"nullX", 5}));
    assert (next_throws_invalid_value (event::null, {"null ", 5}));

    assert (next_throws_invalid_value (event::boolean, {"True", 4}));
    assert (next_throws_invalid_value (event::boolean, {"TRUE", 4}));
    assert (next_throws_invalid_value (event::boolean, {"tru", 3}));
    assert (next_throws_invalid_value (event::boolean, {"trueX", 5}));
    assert (next_throws_invalid_value (event::boolean, {"true ", 5}));

    assert (next_throws_invalid_value (event::boolean, {"False", 5}));
    assert (next_throws_invalid_value (event::boolean, {"FALSE", 5}));
    assert (next_throws_invalid_value (event::boolean, {"fals", 4}));
    assert (next_throws_invalid_value (event::boolean, {"falseX", 6}));
    assert (next_throws_invalid_value (event::boolean, {"false ", 6}));
  }

  // null event: the value is supplied if it is unspecified.
  //
  {
    string b;
    buffer_serializer s (b);
    s.next (event::null);
    assert (b == "null");
  }

  // UTF-8 sequences are not split if buffer runs out of space.
  //
  // Despite there being capacity for the first part of a UTF-8 sequence, none
  // of it must be written.
  //
  {
    uint8_t b[100];
    const string v ("\xE2\x82\xAC"); // U+20AC '€'

    // Using the unchecked version of next().
    //
    {
      size_t n (0);
      buffer_serializer s (b, n, 3);
      assert (next_throws (error::buffer_overflow,
                           s,
                           event::string,
                           {v.c_str (), v.size ()},
                           false));
      assert (n == 1); // Only the opening quote should've been written.
    }

    // Using the checked version of next().
    //
    {
      size_t n (0);
      buffer_serializer s (b, n, 3);
      assert (next_throws (error::buffer_overflow,
                           s,
                           event::string,
                           {v.c_str (), v.size ()},
                           true));
      assert (n == 1);
    }
  }

  // UTF-8 validation.
  //
  {
    assert (serialize_throws ("\xC2"));     // Truncated 2-byte sequence.
    assert (serialize_throws ("\xE1\x80")); // Truncated 3-byte sequence.
    assert (serialize_throws ("\xF1\x80\x80")); // Truncated 4-byte sequence.
    assert (serialize_throws ("\xC0\xB0")); // Overlong encoding of '0' (0x30).
    assert (serialize_throws ("\xC1\xBE")); // Overlong encoding of '~' (0x7E).
    assert (serialize_throws ("\xC2\x7F")); // 2nd byte < valid range.
    assert (serialize_throws ("\xC2\xC0")); // 2nd byte > valid range.

    // Special second-byte cases.
    //
    assert (serialize_throws ("\xE0\x9F\x80")); // 2nd byte < valid range.
    assert (serialize_throws ("\xED\xA0\x80")); // 2nd byte > valid range.
    assert (serialize_throws ("\xF0\x8F\x80\x80")); // 2nd byte < valid range.
    assert (serialize_throws ("\xF4\x90\x80\x80")); // 2nd byte > valid range.
  }

  // Escaping.
  //
  {
    assert (serialize ("\"") == "\\\"");
    assert (serialize ("\\") == "\\\\");
    assert (serialize ("\t") == "\\t");
    assert (serialize ("\n") == "\\n");
    assert (serialize ("\b") == "\\b");
    assert (serialize ("\r") == "\\r");
    assert (serialize ("\f") == "\\f");
    assert (serialize ("\x01") == "\\u0001");
    assert (serialize ("\x1F") == "\\u001F");
    assert (serialize ("ABC \t DEF \x01\x02 GHI") ==
            "ABC \\t DEF \\u0001\\u0002 GHI");
  }

  // Exception offset.
  //
  // The offset stored in the invalid_json_output exception should point to
  // the beginning of the invalid UTF-8 sequence (a truncated 3-byte sequence
  // in this case).
  //
  {
    string b;
    buffer_serializer s (b);
    try
    {
      s.next (event::string, {"abc\xE1\x80", 5}, true);
      assert (false);
    }
    catch (const invalid_json_output& e)
    {
      assert (e.offset == 3);
    }
  }

  // High-level interface.
  //
  {
    // All JSON types.
    //
    {
      string b;
      buffer_serializer s (b, 0);
      s.value ("a");
      s.value (string ("b"));
      s.value (999);
      s.value (nullptr);
      s.value (true);
      assert (b == "\"a\"\n\"b\"\n999\nnull\ntrue");
    }

    // Object.
    //
    {
      string b;
      buffer_serializer s (b, 0);
      s.begin_object ();
      s.member ("a", 1);
      s.member_name ("b"); s.value ("z");
      s.member ("c", string ("y"));
      s.member ("d", nullptr);
      s.member ("e", true);
      s.end_object ();
      assert (b == "{\"a\":1,\"b\":\"z\",\"c\":\"y\",\"d\":null,\"e\":true}");
    }

    // Array.
    {
      string b;
      buffer_serializer s (b, 0);
      s.begin_array ();
      s.value (1);
      s.value ("a");
      s.end_array ();
      assert (b == "[1,\"a\"]");
    }

    // Long floating point numbers should be output in scientific notation.
    // (This also tests that numbers with many digits do not break things.)
    //
    {
      string b;
      buffer_serializer s (b, 0);
      s.value (numeric_limits<long double>::max ());
      assert (b.find ("e+") != string::npos);
    }

    // A null char* is serialized as a JSON null.
    //
    {
      string b;
      buffer_serializer s (b);
      const char* cp (nullptr);
      s.value (cp);
      assert (b == "null");
    }
  }
}
