#include <cstdio>               // snprintf
#include <cstdarg>              // va_list
#include <cstring>              // memcpy
#include <ostream>

#include <libstud/json/serializer.hxx>

using namespace std;

namespace stud
{
  namespace json
  {
    using buffer     = buffer_serializer::buffer;
    using error_code = invalid_json_output::error_code;

    template <typename T>
    static void
    dynarray_overflow (void* d, event, buffer& b, size_t ex)
    {
      T& v (*static_cast<T*> (d));
      v.resize (b.capacity + ex);
      v.resize (v.capacity ());
      // const_cast is required for std::string pre C++17.
      //
      b.data = const_cast<typename T::value_type*> (v.data ());
      b.capacity = v.size ();
    }

    template <typename T>
    static void
    dynarray_flush (void* d, event, buffer& b)
    {
      T& v (*static_cast<T*> (d));
      v.resize (b.size);
      b.data = const_cast<typename T::value_type*> (v.data ());
      b.capacity = b.size;
    }

    buffer_serializer::
    buffer_serializer (string& s, size_t i)
        : buffer_serializer (const_cast<char*> (s.data ()), size_, s.size (),
                             dynarray_overflow<string>,
                             dynarray_flush<string>,
                             &s,
                             i)
    {
      size_ = s.size ();
    }

    buffer_serializer::
    buffer_serializer (vector<char>& v, size_t i)
        : buffer_serializer (v.data (), size_, v.size (),
                             dynarray_overflow<vector<char>>,
                             dynarray_flush<vector<char>>,
                             &v,
                             i)
    {
      size_ = v.size ();
    }

    static void
    ostream_overflow (void* d, event e, buffer& b, size_t)
    {
      ostream& s (*static_cast<ostream*> (d));
      s.write (static_cast<char*> (b.data), b.size);
      if (s.fail ())
        throw invalid_json_output (
            e, error_code::buffer_overflow, "unable to write JSON output text");
      b.size = 0;
    }

    static void
    ostream_flush (void* d, event e, buffer& b)
    {
      ostream_overflow (d, e, b, 0);

      ostream& s (*static_cast<ostream*> (d));
      s.flush ();
      if (s.fail ())
        throw invalid_json_output (
            e, error_code::buffer_overflow, "unable to write JSON output text");
    }

    stream_serializer::
    stream_serializer (ostream& os, size_t i)
        : buffer_serializer (tmp_, sizeof (tmp_),
                             ostream_overflow,
                             ostream_flush,
                             &os,
                             i)
    {
    }

    bool buffer_serializer::
    next (optional<event> e, pair<const char*, size_t> val, bool check)
    {
      if (absent_ == 2)
        goto fail_complete;

      if (e == nullopt)
      {
        if (!state_.empty ())
          goto fail_incomplete;

        absent_++;
        return false;
      }

      absent_ = 0; // Clear inter-value absent event.

      {
        state* st (state_.empty () ? nullptr : &state_.back ());

        auto name_expected = [] (const state& s)
        {
          return s.type == event::begin_object && s.count % 2 == 0;
        };

        auto make_str = [] (const char* s, size_t n)
        {
          return make_pair (s, n);
        };

        // When it comes to pretty-printing, the common way to do it is along
        // these lines:
        //
        // {
        //   "str": "value",
        //   "obj": {
        //     "arr": [
        //       1,
        //       2,
        //       3
        //     ]
        //   },
        //   "num": 123
        // }
        //
        // Empty objects and arrays are printed without a newline:
        //
        // {
        //   "obj": {},
        //   "arr": []
        // }
        //
        // There are two types of separators: between name and value, which is
        // always ": ", and before/after value inside an object or array which
        // is either newline followed by indentation, or comma followed by
        // newline followed by indentation (we also have separation between
        // top-level values but that's orthogonal to pretty-printing).
        //
        // Based on this observation, we are going to handle the latter case by
        // starting with the ",\n" string (in this->sep_) and pushing/popping
        // indentation spaces as we enter/leave objects and arrays. We handle
        // the cases where we don't need the comma by simply skipping it in the
        // C-string pointer.
        //
        bool pp (indent_ != 0);

        pair<const char*, size_t> sep;
        if (st != nullptr)
        {
          // The name-value separator.
          //
          if (st->type == event::begin_object && st->count % 2 == 1)
          {
            sep = !pp ? make_str (":", 1) : make_str (": ", 2);
          }
          // We don't need the comma if we are closing the object or array.
          //
          else if (e == event::end_array || e == event::end_object)
          {
            // But in this case we need to unindent one level prior to writing
            // the brace. Also handle the empty object/array as a special case.
            //
            sep = !pp || st->count == 0
              ? make_str (nullptr, 0)
              : make_str (sep_.c_str () + 1, sep_.size () - 1 - indent_);
          }
          // Or if this is the first value (note: must come after end_*).
          //
          else if (st->count == 0)
          {
            sep = !pp
              ? make_str (nullptr, 0)
              : make_str (sep_.c_str () + 1, sep_.size () - 1);
          }
          else
          {
            sep = !pp
              ? make_str (",", 1)
              : make_str (sep_.c_str (), sep_.size ());
          }
        }
        else if (values_ != 0) // Subsequent top-level value.
        {
          // Top-level value separation. For now we always separate them with
          // newlines, which is the most common/sensible way.
          //
          sep = make_str ("\n", 1);
        }

        switch (*e)
        {
        case event::begin_array:
        case event::begin_object:
          {
            if (st != nullptr && name_expected (*st))
              goto fail_unexpected_event;

            write (*e,
                   sep,
                   make_str (e == event::begin_array ? "[" : "{", 1),
                   false);

            if (st != nullptr)
              st->count++;

            if (pp)
              sep_.append (indent_, ' ');

            state_.push_back (state {*e, 0});
            break;
          }
        case event::end_array:
        case event::end_object:
          {
            if (st == nullptr || (e == event::end_array
                                  ? st->type != event::begin_array
                                  : !name_expected (*st)))
              goto fail_unexpected_event;

            write (*e,
                   sep,
                   make_str (e == event::end_array ? "]" : "}", 1),
                   false);

            if (pp)
              sep_.erase (sep_.size () - indent_);

            state_.pop_back ();
            break;
          }
        case event::name:
        case event::string:
          {
            if (e == event::name
                ? (st == nullptr || !name_expected (*st))
                : (st != nullptr && name_expected (*st)))
              goto fail_unexpected_event;

            write (*e, sep, val, check, '"');

            if (st != nullptr)
              st->count++;
            break;
          }
        case event::null:
        case event::boolean:
          {
            if (e == event::null && val.first == nullptr)
              val = {"null", 4};
            else if (check)
            {
              auto eq = [&val] (const char* v, size_t n)
              {
                return val.second == n && memcmp (val.first, v, n) == 0;
              };

              if (e == event::null)
              {
                if (!eq ("null", 4))
                  goto fail_null;
              }
              else
              {
                if (!eq ("true", 4) && !eq ("false", 5))
                  goto fail_bool;
              }
            }
          }
          // Fall through.
        case event::number:
          {
            if (st != nullptr && name_expected (*st))
              goto fail_unexpected_event;

            write (*e, sep, val, check);

            if (st != nullptr)
              st->count++;
            break;
          }
        }
      }

      if (state_.empty ())
      {
        values_++;
        if (flush_ != nullptr)
          flush_ (data_, *e, buf_);

        return false;
      }

      return true;

    fail_complete:
      throw invalid_json_output (
          e, error_code::invalid_value, "value sequence is complete");
    fail_incomplete:
      throw invalid_json_output (
          e, error_code::invalid_value, "value is incomplete");
    fail_null:
      throw invalid_json_output (
          e, error_code::invalid_value, "invalid null value");
    fail_bool:
      throw invalid_json_output (
          e, error_code::invalid_value, "invalid boolean value");
    fail_unexpected_event:
      throw invalid_json_output (
          e, error_code::unexpected_event, "unexpected event");
    }

    // JSON escape sequences for control characters <= 0x1F.
    //
    static const char* json_escapes[] =
    {"\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005",
     "\\u0006", "\\u0007", "\\b",     "\\t",     "\\n",     "\\u000B",
     "\\f",     "\\r",     "\\u000E", "\\u000F", "\\u0010", "\\u0011",
     "\\u0012", "\\u0013", "\\u0014", "\\u0015", "\\u0016", "\\u0017",
     "\\u0018", "\\u0019", "\\u001A", "\\u001B", "\\u001C", "\\u001D",
     "\\u001E", "\\u001F"};

    void buffer_serializer::
    write (event e,
           pair<const char*, size_t> sep,
           pair<const char*, size_t> val,
           bool check,
           char q)
    {
      // Assumptions:
      //
      // 1. A call to overflow should be able to provide enough capacity to
      //    write the entire separator (in other words, we are not going to
      //    bother with chunking the separator).
      //
      // 2. Similarly, a call to overflow should be able to provide enough
      //    capacity to write an entire UTF-8 multi-byte sequence.
      //
      // 3. Performance-wise, we do not expect very long contiguous sequences
      //    of character that require escaping.

      // Total number of bytes remaining to be written and the capacity
      // currently available.
      //
      size_t size (sep.second + val.second + (q != '\0' ? 2 : 0));
      size_t cap (buf_.capacity - buf_.size);

      auto grow = [this, e, &size, &cap] (size_t min, size_t extra = 0)
      {
        if (overflow_ == nullptr)
          return false;

        extra += size;
        extra -= cap;
        overflow_ (data_, e, buf_, extra > min ? extra : min);
        cap = buf_.capacity - buf_.size;

        return cap >= min;
      };

      auto append = [this, &cap, &size] (const char* d, size_t s)
      {
        memcpy (static_cast<char*> (buf_.data) + buf_.size, d, s);
        buf_.size += s;
        cap -= s;
        size -= s;
      };

      // Return the longest chunk of input that fits into the buffer and does
      // not end in the middle of a multi-byte UTF-8 sequence. Assume value
      // size and capacity are not 0. Return NULL in first if no chunk could
      // be found that fits into the remaining space. In this case, second is
      // the additional (to size) required space (used to handle escapes in
      // the checked version).
      //
      // The basic idea is to seek in the input buffer to the capacity of the
      // output buffer (unless the input is shorter than the output). If we
      // ended up in the middle of a multi-byte UTF-8 sequence, then seek back
      // until we end up at the UTF-8 sequence boundary. Note that this
      // implementation assumes valid UTF-8.
      //
      auto chunk = [&cap, &val] () -> pair<const char*, size_t>
      {
        pair<const char*, size_t> r (nullptr, 0);

        if (cap >= val.second)
          r = val;
        else
        {
          // Start from the character past capacity and search for a UTF-8
          // sequence boundary.
          //
          for (const char* p (val.first + cap); p != val.first; --p)
          {
            const auto u (static_cast<uint8_t> (*p));
            if (u < 0x80 || u > 0xBF) // Not a continuation byte
            {
              r = {val.first, p - val.first};
              break;
            }
          }
        }

        val.first += r.second;
        val.second -= r.second;

        return r;
      };

      // Escaping and UTF-8-validating version of chunk().
      //
      // There are three classes of mandatory escapes in a JSON string:
      //
      // - \\ and \"
      //
      // - \b \f \n \r \t for popular control characters
      //
      // - \u00NN for other control characters <= 0x1F
      //
      // If the input begins with a character that must be escaped, return
      // only its escape sequence. Otherwise validate and return everything up
      // to the end of input or buffer capacity, but cutting it short before
      // the next character that must be escaped or the first UTF-8 sequence
      // that would not fit.
      //
      // Return string::npos in second in case of a stray continuation byte or
      // any byte in an invalid UTF-8 range (for example, an "overlong" 2-byte
      // encoding of a 7-bit/ASCII character or a 4-, 5-, or 6-byte sequence
      // that would encode a codepoint beyond the U+10FFFF Unicode limit).
      //
      auto chunk_checked = [&cap, &size, &val] () -> pair<const char*, size_t>
      {
        pair<const char*, size_t> r (nullptr, 0);

        // Check whether the first character needs to be escaped.
        //
        const uint8_t c (val.first[0]);
        if (c == '"')
          r = {"\\\"", 2};
        else if (c == '\\')
          r = {"\\\\", 2};
        else if (c <= 0x1F)
        {
          auto s (json_escapes[c]);
          r = {s, s[1] == 'u' ? 6 : 2};
        }

        if (r.first != nullptr)
        {
          // Return in second the additional (to size) space required.
          //
          if (r.second > cap)
            return {nullptr, r.second - 1};

          // If we had to escape the character then adjust size accordingly
          // (see append() above).
          //
          size += r.second - 1;

          val.first += 1;
          val.second -= 1;
          return r;
        }

        // First character doesn't need to be escaped. Return as much of the
        // rest of the input as possible.
        //
        size_t i (0);
        for (size_t n (min (cap, val.second)); i != n; i++)
        {
          const uint8_t c1 (val.first[i]);

          if (c1 == '"' || c1 == '\\' || c1 <= 0x1F) // Needs to be escaped.
            break;
          else if (c1 >= 0x80) // Not ASCII, so validate as a UTF-8 sequence.
          {
            size_t i1 (i); // Position of the first byte.

            // The control flow here is to continue if valid and to fall
            // through to return on error.
            //
            if (c1 >= 0xC2 && c1 <= 0xDF) // 2-byte sequence.
            {
              if (i + 2 <= val.second) // Sequence is complete in JSON value.
              {
                if (i + 2 > cap) // Sequence won't fit.
                  break;

                const uint8_t c2 (val.first[++i]);

                if (c2 >= 0x80 && c2 <= 0xBF)
                  continue;
              }
            }
            else if (c1 >= 0xE0 && c1 <= 0xEF) // 3-byte sequence.
            {
              if (i + 3 <= val.second)
              {
                if (i + 3 > cap)
                  break;

                const uint8_t c2 (val.first[++i]), c3 (val.first[++i]);

                if (c3 >= 0x80 && c3 <= 0xBF)
                {
                  switch (c1)
                  {
                  case 0xE0: if (c2 >= 0xA0 && c2 <= 0xBF) continue; break;
                  case 0xED: if (c2 >= 0x80 && c2 <= 0x9F) continue; break;
                  default:   if (c2 >= 0x80 && c2 <= 0xBF) continue; break;
                  }
                }
              }
            }
            else if (c1 >= 0xF0 && c1 <= 0xF4) // 4-byte sequence.
            {
              if (i + 4 <= val.second)
              {
                if (i + 4 > cap)
                  break;

                const uint8_t c2 (val.first[++i]),
                              c3 (val.first[++i]),
                              c4 (val.first[++i]);

                if (c3 >= 0x80 && c3 <= 0xBF &&
                    c4 >= 0x80 && c4 <= 0xBF)
                {
                  switch (c1)
                  {
                  case 0xF0: if (c2 >= 0x90 && c2 <= 0xBF) continue; break;
                  case 0xF4: if (c2 >= 0x80 && c2 <= 0x8F) continue; break;
                  default:   if (c2 >= 0x80 && c2 <= 0xBF) continue; break;
                  }
                }
              }
            }

            r = {val.first, string::npos};

            // Update val to point to the beginning of the invalid sequence.
            //
            val.first += i1;
            val.second -= i1;

            return r;
          }
        }

        if (i != 0) // We have a chunk.
        {
          r = {val.first, i};

          val.first += i;
          val.second -= i;
        }

        return r;
      };

      // Value's original size (used to calculate the offset of the errant
      // character in case of a validation failure).
      //
      const size_t vn (val.second);

      // Write the separator, if any.
      //
      if (sep.second != 0)
      {
        if (cap < sep.second && !grow (sep.second))
          goto fail_nospace;

        append (sep.first, sep.second);
      }

      // Write the value's opening quote, if requested.
      //
      if (q != '\0')
      {
        if (cap == 0 && !grow (1))
          goto fail_nospace;

        append ("\"", 1);
      }

      // Write the value, unless empty.
      //
      while (val.second != 0)
      {
        pair<const char*, size_t> ch (nullptr, 0);

        if (cap != 0)
          ch = check ? chunk_checked () : chunk ();

        if (ch.first == nullptr)
        {
          // The minimum extra bytes we need the overflow function to be able
          // to provide is based on these sequences that we do not break:
          //
          // - 4 bytes for a UTF-8 sequence
          // - 6 bytes for an escaped Unicode sequence (\uXXXX).
          //
          if (!grow (6, ch.second))
            goto fail_nospace;
        }
        else if (ch.second != string::npos)
          append (ch.first, ch.second);
        else
          goto fail_utf8;
      }

      // Write the value's closing quote, if requested.
      //
      if (q != '\0')
      {
        if (cap == 0 && !grow (1))
          goto fail_nospace;

        append ("\"", 1);
      }

      return;

      // Note: keep descriptions consistent with the parser.
      //
    fail_utf8:
      throw invalid_json_output (e,
                                 e == event::name ? error_code::invalid_name
                                                  : error_code::invalid_value,
                                 "invalid UTF-8 text",
                                 vn - val.second);

    fail_nospace:
      throw invalid_json_output (
          e, error_code::buffer_overflow, "insufficient space in buffer");
    }

    size_t buffer_serializer::
    to_chars_impl (char* b, size_t n, const char* f, ...)
    {
      va_list a;
      va_start (a, f);
      const int r (vsnprintf (b, n, f, a));
      va_end (a);

      if (r < 0 || r >= static_cast<int> (n))
      {
        throw invalid_json_output (event::number,
                                   error_code::invalid_value,
                                   "unable to convert number to string");
      }

      return static_cast<size_t> (r);
    }
  }
}
