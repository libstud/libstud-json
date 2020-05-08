#pragma once

#include <array>
#include <iosfwd>
#include <string>
#include <vector>
#include <cstddef>     // size_t, nullptr_t
#include <utility>     // pair
#include <optional>
#include <stdexcept>   // invalid_argument
#include <type_traits> // enable_if, is_*

#include <libstud/json/event.hxx>

#include <libstud/json/export.hxx>

namespace stud
{
  // Using the RFC8259 terminology (JSON (output) text, JSON value, object
  // member).
  //
  namespace json
  {
    class invalid_json_output: public std::invalid_argument
    {
    public:
      using event_type = json::event;

      enum class error_code
      {
        buffer_overflow,
        unexpected_event,
        invalid_name,
        invalid_value
      };

      invalid_json_output (std::optional<event_type> event,
                           error_code code,
                           const char* description,
                           std::size_t offset = std::string::npos);

      invalid_json_output (std::optional<event_type> event,
                           error_code code,
                           const std::string& description,
                           std::size_t offset = std::string::npos);

      // Event that triggered the error. If the error is in the value, then
      // offset points to the offending byte (for example, the beginning of an
      // invalid UTF-8 byte sequence). Otherwise, offset is string::npos.
      //
      std::optional<event_type> event;
      error_code                code;
      std::size_t               offset;
    };

    // The serializer makes sure the resulting JSON is syntactically but not
    // necessarily semantically correct. For example, it's possible to
    // serialize a number event with non-numeric data.
    //
    // Note that unlike the parser, the serializer is always in the multi-
    // value mode allowing the serialization of zero or more values. Note also
    // that while values are separated with newlines, there is no trailing
    // newline after the last (or only) value and the user is expected to add
    // it manually if needed.
    //
    // Also note that while RFC8259 recommends object members to have unique
    // names, the serializer does not enforce this.
    //
    class LIBSTUD_JSON_SYMEXPORT buffer_serializer
    {
    public:
      // Serialize to string growing it as necessary.
      //
      // The indentation argument specifies the number of indentation spaces
      // that should be used for pretty-printing. If 0 is passed, no
      // pretty-printing is performed.
      //
      explicit
      buffer_serializer (std::string&, std::size_t indentation = 2);

      // Serialize to vector of characters growing it as necessary.
      //
      explicit
      buffer_serializer (std::vector<char>&, std::size_t indentation = 2);

      // Serialize to a fixed array.
      //
      // The length of the output text written is tracked in the size
      // argument.
      //
      // If the array is not big enough to store the entire output text, the
      // next() call that reaches the limit will throw invalid_json_output.
      //
      template <std::size_t N>
      buffer_serializer (std::array<char, N>&, std::size_t& size,
                         std::size_t indentation = 2);

      // Serialize to a fixed buffer.
      //
      // The length of the output text written is tracked in the size
      // argument.
      //
      // If the buffer is not big enough to store the entire output text, the
      // next() call that reaches the limit will throw invalid_json_output.
      //
      buffer_serializer (void* buf, std::size_t& size, std::size_t capacity,
                         std::size_t indentation = 2);

      // The overflow function is called when the output buffer is out of
      // space. The extra argument is a hint indicating the extra space likely
      // to be required.
      //
      // Possible strategies include re-allocating a larger buffer or flushing
      // the contents of the original buffer to the output destination. In
      // case of a reallocation, the implementation is responsible for copying
      // the contents of the original buffer over.
      //
      // The flush function is called when the complete JSON value has been
      // serialized to the buffer. It can be used to write the contents of the
      // buffer to the output destination. Note that flush is not called after
      // the second absent (nullopt) event (or the only absent event; see
      // next() for details).
      //
      // Both functions are passed the original buffer, its size (the amount
      // of output text), and its capacity. They return (by modifying the
      // argument) the replacement buffer and its size and capacity (these may
      // refer to the original buffer). If space cannot be made available, the
      // implementation can throw an appropriate exception (for example,
      // std::bad_alloc or std::ios_base::failure). Any exceptions thrown is
      // propagated to the user.
      //
      struct buffer
      {
        void*        data;
        std::size_t& size;
        std::size_t  capacity;
      };

      using overflow_function = void (void* data,
                                      event,
                                      buffer&,
                                      std::size_t extra);
      using flush_function    = void (void* data, event, buffer&);

      // Serialize using a custom buffer and overflow/flush functions (both
      // are optional).
      //
      buffer_serializer (void* buf, std::size_t capacity,
                         overflow_function*,
                         flush_function*,
                         void* data,
                         std::size_t indentation = 2);

      // As above but the length of the output text written is tracked in the
      // size argument.
      //
      buffer_serializer (void* buf, std::size_t& size, std::size_t capacity,
                         overflow_function*,
                         flush_function*,
                         void* data,
                         std::size_t indentation = 2);

      // Begin/end an object.
      //
      void
      begin_object ();

      void
      end_object ();

      // Serialize an object member (name and value).
      //
      // If check is false, then don't check whether the name (or value, if
      // it's a string) is valid UTF-8 and don't escape any characters.
      //
      template <typename T>
      void
      member (const char* name, const T& value, bool check = true);

      template <typename T>
      void
      member (const std::string& name, const T& value, bool check = true);

      // Serialize an object member name.
      //
      // If check is false, then don't check whether the name is valid UTF-8
      // and don't escape any characters.
      //
      void
      member_name (const char*, bool check = true);

      void
      member_name (const std::string&, bool check = true);

      // Begin/end an array.
      //
      void
      begin_array ();

      void
      end_array ();

      // Serialize a string.
      //
      // If check is false, then don't check whether the value is valid UTF-8
      // and don't escape any characters.
      //
      // Note that a NULL C-string pointer is serialized as a null value.
      //
      void
      value (const char*, bool check = true);

      void
      value (const std::string&, bool check = true);

      // Serialize a number.
      //
      template <typename T>
      typename std::enable_if<std::is_integral<T>::value ||
                              std::is_floating_point<T>::value>::type
      value (T);

      // Serialize a boolean value.
      //
      void
      value (bool);

      // Serialize a null value.
      //
      void
      value (std::nullptr_t);

      // Serialize next JSON event.
      //
      // If check is false, then don't check whether the value is valid UTF-8
      // and don't escape any characters.
      //
      // Return true if more events are required to complete the (top-level)
      // value (that is, it is currently incomplete) and false otherwise.
      // Throw invalid_json_output exception in case of an invalid event or
      // value.
      //
      // At the end of the value an optional absent (nullopt) event can be
      // serialized to verify the value is complete. If it is incomplete an
      // invalid_json_output exception is thrown. An optional followup absent
      // event can be serialized to indicate the completion of a multi-value
      // sequence (one and only absent event indicates a zero value sequence).
      // If anything is serialized to a complete value sequence an
      // invalid_json_output exception is thrown.
      //
      // Note that this function was designed to be easily invoked with the
      // output from parser::next() and parser::data(). For example, for a
      // single-value mode:
      //
      //   optional<event> e;
      //   do
      //   {
      //     e = p.next ();
      //     s.next (e, p.data ());
      //   }
      //   while (e);
      //
      // For a multi-value mode:
      //
      //   while (p.peek ())
      //   {
      //     optional<event> e;
      //     do
      //     {
      //       e = p.next ();
      //       s.next (e, p.data ());
      //     }
      //     while (e);
      //   }
      //   s.next (nullopt); // End of value sequence.
      //
      bool
      next (std::optional<event> event,
            std::pair<const char*, std::size_t> value = {},
            bool check = true);

    private:
      void
      write (event,
             std::pair<const char*, std::size_t> sep,
             std::pair<const char*, std::size_t> val,
             bool check, char quote = '\0');

      // Forward a value(v, check) call to value(v) ignoring the check
      // argument. Used in the member() implementation.
      //
      template <typename T>
      void
      value (const T& v, bool /*check*/)
      {
        value (v);
      }

      // Convert numbers to string.
      //
      static std::size_t to_chars (char*, std::size_t, int);
      static std::size_t to_chars (char*, std::size_t, long);
      static std::size_t to_chars (char*, std::size_t, long long);
      static std::size_t to_chars (char*, std::size_t, unsigned int);
      static std::size_t to_chars (char*, std::size_t, unsigned long);
      static std::size_t to_chars (char*, std::size_t, unsigned long long);
      static std::size_t to_chars (char*, std::size_t, double);
      static std::size_t to_chars (char*, std::size_t, long double);

      static std::size_t to_chars_impl (char*, size_t, const char* fmt, ...);

      buffer buf_;
      std::size_t size_;
      overflow_function* overflow_;
      flush_function* flush_;
      void* data_;

      // State of a "structured type" (array or object; as per the RFC
      // terminology).
      //
      struct state
      {
        const event type;  // Type kind (begin_array or begin_object).
        std::size_t count; // Number of events serialized inside this type.
      };

      // Stack of nested structured type states.
      //
      // @@ TODO: would have been nice to use small_vector.
      //
      std::vector<state> state_;

      // The number of consecutive absent events (nullopt) serialized thus
      // far.
      //
      // Note: initialized to 1 to naturally handle a single absent event
      // (declares an empty value sequence complete).
      //
      std::size_t absent_ = 1;

      // The number of spaces with which to indent (once for each level of
      // nesting). If zero, pretty-printing is disabled.
      //
      std::size_t indent_;

      // Separator and indentation before/after value inside an object or
      // array (see pretty-printing implementation for details).
      //
      std::string sep_;

      // The number of complete top-level values serialized thus far.
      //
      std::size_t values_ = 0;
    };

    class LIBSTUD_JSON_SYMEXPORT stream_serializer: public buffer_serializer
    {
    public:
      // Serialize to std::ostream.
      //
      // If stream exceptions are enabled then the std::ios_base::failure
      // exception is used to report input/output errors (badbit and failbit).
      // Otherwise, those are reported as the invalid_json_output exception.
      //
      explicit
      stream_serializer (std::ostream&, std::size_t indentation = 2);

    protected:
      char tmp_[4096];
    };
  }
}

#include <libstud/json/serializer.ixx>
