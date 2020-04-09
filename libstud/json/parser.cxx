#define PDJSON_SYMEXPORT static // See below.

#include <libstud/json/parser.hxx>

#include <istream>

namespace stud
{
  namespace json
  {
    using namespace std;

    parser::
    ~parser ()
    {
      json_close (impl_);
    }

    static int
    stream_get (void* x)
    {
      auto& s (*static_cast<parser::stream*> (x));

      try
      {
        // We first peek not to trip failbit on EOF.
        //
        if (s.is->peek () != istream::traits_type::eof ())
          return static_cast<char> (s.is->get ());
      }
      catch (...)
      {
        s.exception = current_exception ();
      }

      return EOF;
    }

    static int
    stream_peek (void* x)
    {
      auto& s (*static_cast<parser::stream*> (x));

      try
      {
        auto c (s.is->peek ());
        if (c != istream::traits_type::eof ())
          return static_cast<char> (c);
      }
      catch (...)
      {
        s.exception = current_exception ();
      }

      return EOF;
    }

    // NOTE: watch out for exception safety (specifically, doing anything that
    // might throw after opening the stream).
    //
    parser::
    parser (istream& is, const char* n)
        : input_name (n),
          stream_ {&is, nullptr},
          raw_s_ (nullptr), raw_n_ (0)
    {
      json_open_user (impl_, &stream_get, &stream_peek, &stream_);
      json_set_streaming (impl_, false);
    }

    parser::
    parser (const void* t, size_t s, const char* n)
        : input_name (n),
          stream_ {nullptr, nullptr},
          raw_s_ (nullptr), raw_n_ (0)
    {
      json_open_buffer (impl_, t, s);
      json_set_streaming (impl_, false);
    }

    optional<event> parser::
    next ()
    {
      name_p_ = value_p_ = line_p_ = false;

      // Note that for now we don't worry about the state of the parser if
      // next_impl() throws assuming it is not going to be reused.
      //
      if (peeked_)
      {
        parsed_ = peeked_;
        peeked_ = nullopt;
      }
      else
        parsed_ = next_impl ();

      return translate (*parsed_);
    }

    optional<event> parser::
    peek ()
    {
      if (!peeked_)
      {
        if (parsed_)
        {
          cache_parsed_data ();
          cache_parsed_line ();
        }
        peeked_ = next_impl ();
      }
      return translate (*peeked_);
    }

    json_type parser::
    next_impl ()
    {
      raw_s_ = nullptr;
      raw_n_ = 0;

      const json_type e (json_next (impl_));

      // First check for a pending input/output error.
      //
      if (stream_.is != nullptr)
      {
        if (stream_.exception != nullptr)
          goto fail_rethrow;

        if (stream_.is->fail ())
          goto fail_stream;
      }

      switch (e)
      {
      case JSON_ERROR: goto fail_json;
      case JSON_STRING:
      case JSON_NUMBER:
        raw_s_ = json_get_string (impl_, &raw_n_);
        raw_n_--; // Includes terminating `\0`.
        break;
      case JSON_TRUE:  raw_s_ = "true";  raw_n_ = 4; break;
      case JSON_FALSE: raw_s_ = "false"; raw_n_ = 5; break;
      case JSON_NULL:  raw_s_ = "null";  raw_n_ = 4; break;
      default: break;
      }

      return e;

    fail_json:
      throw invalid_json (input_name != nullptr ? input_name : "",
                          static_cast<uint64_t> (json_get_lineno (impl_)),
                          0 /* column */,
                          json_get_error (impl_));

    fail_stream:
      throw invalid_json (input_name != nullptr ? input_name : "",
                          0 /* line */,
                          0 /* column */,
                          "unable to read text");

    fail_rethrow:
      rethrow_exception (move (stream_.exception));
    }

    std::optional<event> parser::
    translate (json_type e) const noexcept
    {
      switch (e)
      {
      case JSON_DONE: return std::nullopt;
      case JSON_OBJECT: return event::begin_object;
      case JSON_OBJECT_END: return event::end_object;
      case JSON_ARRAY: return event::begin_array;
      case JSON_ARRAY_END: return event::end_array;
      case JSON_STRING:
        {
          // This can be a value or, inside an object, a name from the
          // name/value pair.
          //
          size_t n;
          return json_get_context (const_cast<json_stream*> (impl_), &n) ==
                             JSON_OBJECT &&
                         n % 2 == 1
                     ? event::name
                     : event::string;
        }
      case JSON_NUMBER: return event::number;
      case JSON_TRUE: return event::boolean;
      case JSON_FALSE: return event::boolean;
      case JSON_NULL: return event::null;
      case JSON_ERROR: assert (false); // Should've been handled by caller.
      }

      return nullopt; // Should never reach.
    }

    void parser::
    cache_parsed_data ()
    {
      name_p_ = value_p_ = false;
      if (const std::optional<event> e = translate (*parsed_))
      {
        if (e == event::name)
        {
          name_.assign (raw_s_, raw_n_);
          name_p_ = true;
        }
        else if (value_event (e))
        {
          value_.assign (raw_s_, raw_n_);
          value_p_ = true;
        }
      }
    }

    void parser::
    cache_parsed_line () noexcept
    {
      line_ = static_cast<uint64_t> (json_get_lineno (impl_));
      line_p_ = true;
    }

    bool parser::
    value_event (std::optional<event> e) noexcept
    {
      if (!e)
        return false;

      switch (*e)
      {
      case event::string:
      case event::number:
      case event::boolean:
      case event::null:
        return true;
      default:
        return false;
      }
    }

    [[noreturn]] void parser::
    throw_invalid_value (const char* type, const char* v, size_t n) const
    {
      string d (string ("invalid ") + type + " value: '");
      d.append (v, n);
      d += '\'';

      throw invalid_json (input_name != nullptr ? input_name : "",
                          line (),
                          0 /* column */,
                          move (d));
    }
  } // namespace json
} // namespace stud

// Include the implementation into our translation unit (instead of compiling
// it separately) to (hopefully) get function inlining without LTO.
//
// Let's keep it last since the implementation defines a couple of macros.
//
#if defined(__clang__) || defined (__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

extern "C"
{
#define PDJSON_STACK_INC 16
#define PDJSON_STACK_MAX 2048
#include "pdjson.c"
}

#if defined(__clang__) || defined (__GNUC__)
#  pragma GCC diagnostic pop
#endif
