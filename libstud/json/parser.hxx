#pragma once

#include <iosfwd>
#include <string>
#include <cstddef>   // size_t
#include <cstdint>   // uint64_t
#include <utility>   // move()
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
    // @@ TODO: better name?
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
      // If stream exceptions are enabled then std::ios_base::failure
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

      parser (parser&&) = delete;
      parser (const parser&) = delete;

      parser& operator= (parser&&) = delete;
      parser& operator= (const parser&) = delete;

      // Parsing.
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

      // Return the object member name.
      //
      const std::string&
      name ();

      // Any value (string, number, boolean, and null) can be retrieved as a
      // string.
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
      value ();

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
      throw_invalid_value (const char* type);

      ~parser ();

    private:
      stream stream_;

      std::string name_;
      std::string value_;

      ::json_stream impl_[1];

      // Cached raw value.
      //
      const char* raw_s_;
      size_t      raw_n_;
    };
  }
}

#include <libstud/json/parser.ixx>
