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

      // In the multi-value mode reading of whitespaces/separators is split
      // between our code and pdjson's. As a result, these functions may end
      // up being called more than once after EOF is reached. Which is
      // something iostream does not handle gracefully.
      //
      if (!s.is->eof ())
      {
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
      }

      return EOF;
    }

    static int
    stream_peek (void* x)
    {
      auto& s (*static_cast<parser::stream*> (x));

      if (!s.is->eof ())
      {
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
      }

      return EOF;
    }

    // NOTE: watch out for exception safety (specifically, doing anything that
    // might throw after opening the stream).
    //
    parser::
    parser (istream& is, const char* n, bool mv, const char* sep) noexcept
        : input_name (n),
          stream_ {&is, nullptr},
          multi_value_ (mv),
          separators_ (sep),
          raw_s_ (nullptr),
          raw_n_ (0)
    {
      json_open_user (impl_, &stream_get, &stream_peek, &stream_);
      json_set_streaming (impl_, multi_value_);
    }

    parser::
    parser (const void* t,
            size_t s,
            const char* n,
            bool mv,
            const char* sep) noexcept
        : input_name (n),
          stream_ {nullptr, nullptr},
          multi_value_ (mv),
          separators_ (sep),
          raw_s_ (nullptr),
          raw_n_ (0)
    {
      json_open_buffer (impl_, t, s);
      json_set_streaming (impl_, multi_value_);
    }

    optional<event> parser::
    next ()
    {
      name_p_ = value_p_ = location_p_ = false;

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
          cache_parsed_location ();
        }
        peeked_ = next_impl ();
      }
      return translate (*peeked_);
    }

    std::uint64_t parser::
    line () const noexcept
    {
      if (!location_p_)
      {
        if (!parsed_)
          return 0;

        assert (!peeked_);

        return static_cast<uint64_t> (
            json_get_lineno (const_cast<json_stream*> (impl_)));
      }

      return line_;
    }

    std::uint64_t parser::
    column () const noexcept
    {
      if (!location_p_)
      {
        if (!parsed_)
          return 0;

        assert (!peeked_);

        return static_cast<uint64_t> (
            json_get_column (const_cast<json_stream*> (impl_)));
      }

      return column_;
    }

    std::uint64_t parser::
    position () const noexcept
    {
      if (!location_p_)
      {
        if (!parsed_)
          return 0;

        assert (!peeked_);

        return static_cast<uint64_t> (
            json_get_position (const_cast<json_stream*> (impl_)));
      }

      return position_;
    }

    json_type parser::
    next_impl ()
    {
      raw_s_ = nullptr;
      raw_n_ = 0;
      json_type e;

      // Read characters between values skipping required separators and JSON
      // whitespaces. Return whether a required separator was encountered as
      // well as the first non-separator/whitespace character (which, if EOF,
      // should trigger a check for input/output errors).
      //
      // Note that the returned non-separator will not have been extracted
      // from the input (so position, column, etc. will still refer to its
      // predecessor).
      //
      auto skip_separators = [this] () -> pair<bool, int>
      {
        bool r (separators_ == nullptr);

        int c;
        for (; (c = json_source_peek (impl_)) != EOF; json_source_get (impl_))
        {
          // User separator.
          //
          if (separators_ != nullptr && *separators_ != '\0')
          {
            if (strchr (separators_, c) != nullptr)
            {
              r = true;
              continue;
            }
          }

          // JSON separator.
          //
          if (json_isspace (c))
          {
            if (separators_ != nullptr && *separators_ == '\0')
              r = true;

            continue;
          }

          break;
        }

        return make_pair (r, c);
      };

      // In the multi-value mode skip any instances of required separators
      // (and any other JSON whitespace) preceding the first JSON value.
      //
      if (multi_value_ && !parsed_ && !peeked_)
      {
        if (skip_separators ().second == EOF && stream_.is != nullptr)
        {
          if (stream_.exception != nullptr) goto fail_rethrow;
          if (stream_.is->fail ())          goto fail_stream;
        }
      }

      e = json_next (impl_);

      // First check for a pending input/output error.
      //
      if (stream_.is != nullptr)
      {
        if (stream_.exception != nullptr) goto fail_rethrow;
        if (stream_.is->fail ())          goto fail_stream;
      }

      // There are two ways to view separation between two values: as following
      // the first value or as preceding the second value. And one aspect that
      // is determined by this is whether a separation violation is a problem
      // with the first value or with the second, which becomes important if
      // the user bails out before parsing the second value.
      //
      // Consider these two unseparated value (yes, in JSON they are two
      // values, leading zeros are not allowed in JSON numbers):
      //
      // 01
      //
      // If the user bails out after parsing 0 in a stream that should have
      // been newline-delimited, they most likely would want to get an error
      // since this is most definitely an invalid value rather than two
      // values that are not properly separated. So in this light we handle
      // separators at the end of the first value.
      //
      switch (e)
      {
      case JSON_DONE:
        {
          // Deal with the following value separators.
          //
          // Note that we must not do this for the second JSON_DONE (or the
          // first one in case there are no values) that signals the end of
          // input.
          //
          if (multi_value_         &&
              (parsed_ || peeked_) &&
              (peeked_ ? *peeked_ : *parsed_) != JSON_DONE)
          {
            auto p (skip_separators ());

            if (p.second == EOF && stream_.is != nullptr)
            {
              if (stream_.exception != nullptr) goto fail_rethrow;
              if (stream_.is->fail ())          goto fail_stream;
            }

            // Note that we don't require separators after the last value.
            //
            if (!p.first && p.second != EOF)
            {
              json_source_get (impl_); // Consume to update column number.
              goto fail_separation;
            }

            json_reset (impl_);
          }
          break;
        }
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
      throw invalid_json_input (
          input_name != nullptr ? input_name : "",
          static_cast<uint64_t> (json_get_lineno (impl_)),
          static_cast<uint64_t> (json_get_column (impl_)),
          static_cast<uint64_t> (json_get_position (impl_)),
          json_get_error (impl_));

    fail_separation:
      throw invalid_json_input (
          input_name != nullptr ? input_name : "",
          static_cast<uint64_t> (json_get_lineno (impl_)),
          static_cast<uint64_t> (json_get_column (impl_)),
          static_cast<uint64_t> (json_get_position (impl_)),
          "missing separator between JSON values");

    fail_stream:
      throw invalid_json_input (
          input_name != nullptr ? input_name : "",
          static_cast<uint64_t> (json_get_lineno (impl_)),
          static_cast<uint64_t> (json_get_column (impl_)),
          static_cast<uint64_t> (json_get_position (impl_)),
          "unable to read JSON input text");

    fail_rethrow:
      rethrow_exception (move (stream_.exception));
    }

    optional<event> parser::
    translate (json_type e) const noexcept
    {
      switch (e)
      {
      case JSON_DONE: return nullopt;
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
      if (const optional<event> e = translate (*parsed_))
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
    cache_parsed_location () noexcept
    {
      line_ = static_cast<uint64_t> (json_get_lineno (impl_));
      column_ = static_cast<uint64_t> (json_get_column (impl_));
      position_ = static_cast<uint64_t> (json_get_position (impl_));
      location_p_ = true;
    }

    bool parser::
    value_event (optional<event> e) noexcept
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

      throw invalid_json_input (input_name != nullptr ? input_name : "",
                                line (),
                                column (),
                                position (),
                                move (d));
    }
  } // namespace json
} // namespace stud

// Include the implementation into our translation unit (instead of compiling
// it separately) to (hopefully) get function inlining without LTO.
//
// Let's keep it last since the implementation defines a couple of macros.
//
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

extern "C"
{
#define PDJSON_STACK_INC 16
#define PDJSON_STACK_MAX 2048
#include "pdjson.c"
}
