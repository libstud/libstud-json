#pragma once

#include <iosfwd>
#include <string>
#include <cstddef>   // size_t
#include <cstdint>   // uint64_t
#include <utility>   // pair
#include <optional>
#include <exception> // exception_ptr
#include <stdexcept> // invalid_argument

#include <libstud/json/pdjson.h> // Implementation details.

#include <libstud/json/export.hxx>

namespace stud
{
  // Using the RFC8259 terminology (JSON text, JSON value, object member).
  //
  namespace json
  {
    // @@ TODO: add position/offset?
    //
    class invalid_json: public std::invalid_argument
    {
    public:
      std::string   name;
      std::uint64_t line;
      std::uint64_t column;

      invalid_json (std::string name,
                    std::uint64_t line,
                    std::uint64_t column,
                    const std::string& description);

      invalid_json (std::string name,
                    std::uint64_t line,
                    std::uint64_t column,
                    const char* description);
    };

    // Parsing event.
    //
    enum class event
    {
      begin_object, end_object,
      begin_array,  end_array,
      name,
      string,
      number,
      boolean,
      null
    };

    class LIBSTUD_JSON_SYMEXPORT parser
    {
    public:
      const char* input_name;

      // Construction.
      //

      // Parse JSON input text from std::istream.
      //
      // The name argument is used to identify the input being parsed. Note
      // that the stream and name are kept as references so both must outlive
      // the parser instance.
      //
      // If stream exceptions are enabled then the std::ios_base::failure
      // exception is used to report input/output errors (badbit and failbit).
      // Otherwise, those are reported as the invalid_json exception.
      //
      parser (std::istream&, const std::string& name);
      parser (std::istream&, const char* name);
      parser (std::istream&, std::string&&) = delete;

      // Parse a memory buffer that contains the entire JSON input text.
      //
      // The name argument is used to identify the input being parsed. Note
      // that the buffer and name are kept as references so both must outlive
      // the parser instance.
      //
      parser (const void* text, std::size_t size, const std::string& name);
      parser (const void* text, std::size_t size, const char* name);
      parser (const void*, std::size_t, std::string&&) = delete;

      // Similar to the above but parse a string.
      //
      parser (const std::string& text, const std::string& name);
      parser (const std::string& text, const char* name);
      parser (const std::string&, std::string&&) = delete;

      // Similar to the above but parse a C-string.
      //
      parser (const char* text, const std::string& name);
      parser (const char* text, const char* name);
      parser (const char*, std::string&&) = delete;

      parser (parser&&) = delete;
      parser (const parser&) = delete;

      parser& operator= (parser&&) = delete;
      parser& operator= (const parser&) = delete;

      // Return the next event.
      //
      //     while (optional<event> e = p.next ())
      //
      std::optional<event>
      next ();

      // The range-based for loop support.
      //
      //     for (event e: p)
      //
      // Generally, the iterator interface doesn't make much sense for the
      // parser so for now we have an implementation that is just enough for
      // the range-based for.
      //
      struct iterator;

      iterator begin () {return iterator (this, next ());}
      iterator end ()   {return iterator (nullptr, std::nullopt);}

      // Return the next event without considering it parsed. In other words,
      // after this call, any subsequent calls to peek() and the next call to
      // next() (if any) will all return the same event.
      //
      // Note that the name, value, and line corresponding to the peeked event
      // are not accessible with name(), value() and line(); these functions
      // will still return values corresponding to the most recent call to
      // next(). The peeked values, however, can be accessed in the raw form
      // using data().
      //
      std::optional<event>
      peek ();

      // Event data.
      //

      // Return the object member name.
      //
      const std::string&
      name ();

      // Any value (string, number, boolean, and null) can be retrieved as a
      // string. Calling this function after any non-value events is illegal.
      //
      // Note that the value is returned as a non-const string reference and
      // you are allowed to move the value out of it. However, this should not
      // be done unnecessarily or in cases where the small string optimization
      // is likely since the string's buffer is reused to store subsequent
      // values.
      //
      std::string&
      value ();

      // Convert the value to an integer, floating point, or bool. Throw
      // invalid_json if the conversion is impossible without a loss.
      //
      template <typename T>
      T
      value () const;

      // Return the line number corresponding to the most recently parsed
      // event.
      //
      std::uint64_t
      line () const;

      // Return the value or object member name in the raw form.
      //
      // Calling this function on non-value/name events is legal in which case
      // NULL is returned. Note also that the returned data corresponds to the
      // most recent event, whether peeked or parsed.
      //
      std::pair<const char*, std::size_t>
      data () const {return std::make_pair (raw_s_, raw_n_);}

      // Implementation details.
      //
    public:
      struct iterator
      {
        using value_type = event;

        explicit
        iterator (parser* p = nullptr, std::optional<event> e = std::nullopt)
            : p_ (p), e_ (e) {}

        event operator* () const {return *e_;}
        iterator& operator++ () {e_ = p_->next (); return *this;}

        // Comparison only makes sense when comparing to end (eof).
        //
        bool operator== (iterator y) const {return !e_ && !y.e_;}
        bool operator!= (iterator y) const {return !(*this == y);}

      private:
        parser* p_;
        std::optional<event> e_;
      };

      struct stream
      {
        std::istream*      is;
        std::exception_ptr exception;
      };

      [[noreturn]] void
      throw_invalid_value (const char* type, const char*, std::size_t) const;

      ~parser ();

    private:
      // Functionality shared by next() and peek().
      //
      json_type
      next_impl ();

      // Translate the event produced by the most recent call to next_impl().
      //
      // Note that the underlying parser state determines whether name or
      // value is returned when translating JSON_STRING.
      //
      std::optional<event>
      translate (json_type) const noexcept;

      // Cache state (name/value) produced by the most recent call to
      // next_impl().
      //
      void
      cache_parsed_data ();

      // Cache the line number as determined by the most recent call to
      // next_impl().
      void
      cache_parsed_line () noexcept;

      // Return true if this is a value event (string, number, boolean, or
      // null).
      //
      static bool
      value_event (std::optional<event>) noexcept;

      stream stream_;

      // The *_p_ members indicate whether the value is present (cached).
      // Note: not using optional not to reallocate the string's buffer.
      //
      std::string name_;   bool name_p_  = false;
      std::string value_;  bool value_p_ = false;
      std::uint64_t line_; bool line_p_  = false;

      std::optional<json_type> parsed_; // Current parsed event if any.
      std::optional<json_type> peeked_; // Current peeked event if any.

      ::json_stream impl_[1];

      // Cached raw value.
      //
      const char* raw_s_;
      std::size_t raw_n_;
    };
  }
}

#include <libstud/json/parser.ixx>
