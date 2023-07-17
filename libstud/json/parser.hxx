#pragma once

#include <iosfwd>
#include <string>
#include <cstddef>   // size_t
#include <cstdint>   // uint64_t
#include <utility>   // pair
#include <exception> // exception_ptr
#include <stdexcept> // invalid_argument

#include <libstud/optional.hxx> // stud::optional is std::optional or similar.

#include <libstud/json/event.hxx>

#include <libstud/json/pdjson.h> // Implementation details.

#include <libstud/json/export.hxx>

namespace stud
{
  // Using the RFC8259 terminology: JSON (input) text, JSON value, object
  // member.
  //
  namespace json
  {
    class invalid_json_input: public std::invalid_argument
    {
    public:
      std::string   name;
      std::uint64_t line;
      std::uint64_t column;
      std::uint64_t position;

      invalid_json_input (std::string name,
                          std::uint64_t line,
                          std::uint64_t column,
                          std::uint64_t position,
                          const std::string& description);

      invalid_json_input (std::string name,
                          std::uint64_t line,
                          std::uint64_t column,
                          std::uint64_t position,
                          const char* description);
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
      // that the stream, name, and separators are kept as references so they
      // must outlive the parser instance.
      //
      // If stream exceptions are enabled then the std::ios_base::failure
      // exception is used to report input/output errors (badbit and failbit).
      // Otherwise, those are reported as the invalid_json_input exception.
      //
      // If multi_value is true, enable the multi-value mode in which case the
      // input stream may contain multiple JSON values (more precisely, zero
      // or more). If false (the default), parsing will fail unless there is
      // exactly one JSON value in the input stream.
      //
      // If multi_value is true, the separators argument specifies the
      // required separator characters between JSON values. At least one of
      // them must be present between every pair of JSON values (in addition
      // to any number of JSON whitespaces). No separators are required after
      // the last JSON value (but any found will be skipped).
      //
      // Specifically, if it is NULL, then no separation is required (that is,
      // both `{...}{...}` and `{...}  {...}` would be valid). If it is empty,
      // then at least one JSON whitespace is required. And if it is non-
      // empty, then at least one of its characters must be present (for
      // example, "\n\t" would require at least one newline or TAB character
      // between JSON values).
      //
      // Note that a separator need not be valid JSON whitespace: any
      // character is acceptable (though it probably shouldn't be an object,
      // array, or string delimiter and should not occur within a non-self-
      // delimited top-level value, such as `true`, `false`, `null`, or a
      // number). All instances of required separators before and after a
      // value are skipped. Therefore JSON Text Sequences (RFC 7464; AKA
      // Record Separator-delimited JSON), which requires the RS (0x1E)
      // character before each value, can be handled as well.
      //
      parser (std::istream&,
              const std::string& name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (std::istream&,
              const char* name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (std::istream&,
              std::string&&,
              bool = false,
              const char* = nullptr) = delete;

      // Parse a memory buffer that contains the entire JSON input text.
      //
      // The name argument is used to identify the input being parsed. Note
      // that the buffer, name, and separators are kept as references so they
      // must outlive the parser instance.
      //
      parser (const void* text,
              std::size_t size,
              const std::string& name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const void* text,
              std::size_t size,
              const char* name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const void*,
              std::size_t,
              std::string&&,
              bool = false,
              const char* = nullptr) = delete;

      // Similar to the above but parse a string.
      //
      parser (const std::string& text,
              const std::string& name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const std::string& text,
              const char* name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const std::string&,
              std::string&&,
              bool = false,
              const char* = nullptr) = delete;

      // Similar to the above but parse a C-string.
      //
      parser (const char* text,
              const std::string& name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const char* text,
              const char* name,
              bool multi_value = false,
              const char* separators = nullptr) noexcept;

      parser (const char*,
              std::string&&,
              bool = false,
              const char* = nullptr) = delete;

      parser (parser&&) = delete;
      parser (const parser&) = delete;

      parser& operator= (parser&&) = delete;
      parser& operator= (const parser&) = delete;

      // Event iteration.
      //

      // Return the next event or nullopt if end of input is reached.
      //
      // In the single-value parsing mode (default) the parsing code could
      // look like this:
      //
      //     while (optional<event> e = p.next ())
      //     {
      //       switch (*e)
      //       {
      //         // ...
      //       }
      //     }
      //
      // In the multi-value mode the parser additionally returns nullopt after
      // every JSON value parsed (so there will be two nullopt's after the
      // last JSON value, the second indicating the end of input).
      //
      // One way to perform multi-value parsing is with the help of the peek()
      // function (see below):
      //
      //     while (p.peek ())
      //     {
      //       while (optional<event> e = p.next ())
      //       {
      //         switch (*e)
      //         {
      //           //...
      //         }
      //       }
      //     }
      //
      // Note that while the single-value mode will always parse exactly one
      // value, the multi-value mode will accept zero values in which case a
      // single nullopt is returned.
      //
      optional<event>
      next ();

      // The range-based for loop support.
      //
      // In the single-value parsing mode (default) the parsing code could
      // look like this:
      //
      //     for (event e: p)
      //     {
      //       switch (e)
      //       {
      //         //...
      //       }
      //     }
      //
      // And in the multi-value mode (see next() for more information) like
      // this:
      //
      //     while (p.peek ())
      //     {
      //       for (event e: p)
      //       {
      //         switch (e)
      //         {
      //           //...
      //         }
      //       }
      //     }
      //
      // Note that generally, the iterator interface doesn't make much sense
      // for the parser so for now we have an implementation that is just
      // enough for the range-based for.
      //
      struct iterator;

      iterator begin () {return iterator (this, next ());}
      iterator end ()   {return iterator (nullptr, nullopt);}

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
      optional<event>
      peek ();


      // Event data access.
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
      // invalid_json_input if the conversion is impossible without a loss.
      //
      template <typename T>
      T
      value () const;

      // Return the value or object member name in the raw form.
      //
      // Calling this function on non-value/name events is legal in which case
      // NULL is returned. Note also that the returned data corresponds to the
      // most recent event, whether peeked or parsed.
      //
      std::pair<const char*, std::size_t>
      data () const {return std::make_pair (raw_s_, raw_n_);}


      // Higher-level API suitable for parsing specific JSON vocabularies.
      //
      // The API summary:
      //
      // void next_expect (event);
      // bool next_expect (event primary, event secondary);
      //
      // void next_expect_name (string name, bool skip_unknown = false);
      //
      // std::string& next_expect_string    ();
      // T            next_expect_string<T> ();
      // std::string& next_expect_number    ();
      // T            next_expect_number<T> ();
      // std::string& next_expect_boolean   ();
      // T            next_expect_boolean<T>();
      //
      // std::string* next_expect_string_null    ();
      // optional<T>  next_expect_string_null<T> ();
      // std::string* next_expect_number_null    ();
      // optional<T>  next_expect_number_null<T> ();
      // std::string* next_expect_boolean_null   ();
      // optional<T>  next_expect_boolean_null<T>();
      //
      // std::string& next_expect_member_string    (string name, bool = false);
      // T            next_expect_member_string<T> (string name, bool = false);
      // std::string& next_expect_member_number    (string name, bool = false);
      // T            next_expect_member_number<T> (string name, bool = false);
      // std::string& next_expect_member_boolean   (string name, bool = false);
      // T            next_expect_member_boolean<T>(string name, bool = false);
      //
      // std::string* next_expect_member_string_null    (string, bool = false);
      // optional<T>  next_expect_member_string_null<T> (string, bool = false);
      // std::string* next_expect_member_number_null    (string, bool = false);
      // optional<T>  next_expect_member_number_null<T> (string, bool = false);
      // std::string* next_expect_member_boolean_null   (string, bool = false);
      // optional<T>  next_expect_member_boolean_null<T>(string, bool = false);
      //
      // void next_expect_member_object     (string name, bool = false);
      // bool next_expect_member_object_null(string name, bool = false);
      //
      // void next_expect_member_array     (string name, bool = false);
      // bool next_expect_member_array_null(string name, bool = false);
      //
      // void next_expect_value_skip();

      // Get the next event and make sure that it's what's expected: primary
      // or, if specified, secondary event. If it is not either, then throw
      // invalid_json_input with appropriate description. Return true if it is
      // primary.
      //
      // The secondary expected event is primarily useful for handling
      // optional members. For example:
      //
      //     while (p.next_expect (event::name, event::end_object))
      //     {
      //       // Handle object member.
      //     }
      //
      // Or homogeneous arrays:
      //
      //     while (p.next_expect (event::string, event::end_array))
      //     {
      //       // Handle array element.
      //     }
      //
      // Or values that can be null:
      //
      //     if (p.next_expect (event::begin_object, event::null))
      //     {
      //       // Parse object.
      //     }
      //
      bool
      next_expect (event primary, optional<event> secondary = nullopt);

      // Get the next event and make sure it is event::name and the object
      // member matches the specified name. If either is not, then throw
      // invalid_json_input with appropriate description. If skip_unknown is
      // true, then skip over unknown member names until a match is found.
      //
      void
      next_expect_name (const char* name, bool skip_unknown = false);

      void
      next_expect_name (const std::string&, bool = false);

      // Get the next event and make sure it is event::<type> returning its
      // value similar to the value() functions. If it is not, then throw
      // invalid_json_input with appropriate description.
      //
      std::string&
      next_expect_string ();

      template <typename T>
      T
      next_expect_string ();

      std::string&
      next_expect_number ();

      template <typename T>
      T
      next_expect_number ();

      std::string&
      next_expect_boolean ();

      template <typename T>
      T
      next_expect_boolean ();

      // Similar to next_expect_<type>() but in addition to event::<type> also
      // allow event::null, in which case returning no value.
      //
      std::string*
      next_expect_string_null ();

      template <typename T>
      optional<T>
      next_expect_string_null ();

      std::string*
      next_expect_number_null ();

      template <typename T>
      optional<T>
      next_expect_number_null ();

      std::string*
      next_expect_boolean_null ();

      template <typename T>
      optional<T>
      next_expect_boolean_null ();

      // Call next_expect_name() followed by next_expect_<type>[_null]()
      // returning its result. In other words, parse the entire object member
      // with the specifed name and of type <type>, returning its value.

      // next_expect_member_string()
      //
      std::string&
      next_expect_member_string (const char* name, bool skip_unknown = false);

      std::string&
      next_expect_member_string (const std::string&, bool = false);

      template <typename T>
      T
      next_expect_member_string (const char*, bool = false);

      template <typename T>
      T
      next_expect_member_string (const std::string&, bool = false);

      // next_expect_member_number()
      //
      std::string&
      next_expect_member_number (const char* name, bool skip_unknown = false);

      std::string&
      next_expect_member_number (const std::string&, bool = false);

      template <typename T>
      T
      next_expect_member_number (const char*, bool = false);

      template <typename T>
      T
      next_expect_member_number (const std::string&, bool = false);

      // next_expect_member_boolean()
      //
      std::string&
      next_expect_member_boolean (const char* name, bool skip_unknown = false);

      std::string&
      next_expect_member_boolean (const std::string&, bool = false);

      template <typename T>
      T
      next_expect_member_boolean (const char*, bool = false);

      template <typename T>
      T
      next_expect_member_boolean (const std::string&, bool = false);

      // next_expect_member_string_null()
      //
      std::string*
      next_expect_member_string_null (const char*, bool = false);

      std::string*
      next_expect_member_string_null (const std::string&, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_string_null (const char*, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_string_null (const std::string&, bool = false);

      // next_expect_member_number_null()
      //
      std::string*
      next_expect_member_number_null (const char*, bool = false);

      std::string*
      next_expect_member_number_null (const std::string&, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_number_null (const char*, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_number_null (const std::string&, bool = false);

      // next_expect_member_boolean_null()
      //
      std::string*
      next_expect_member_boolean_null (const char*, bool = false);

      std::string*
      next_expect_member_boolean_null (const std::string&, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_boolean_null (const char*, bool = false);

      template <typename T>
      optional<T>
      next_expect_member_boolean_null (const std::string&, bool = false);

      // Call next_expect_name() followed by next_expect(event::begin_object).
      // In the _null version also allow event::null, in which case return
      // false.
      //
      void
      next_expect_member_object (const char* name, bool skip_unknown = false);

      void
      next_expect_member_object (const std::string&, bool = false);

      bool
      next_expect_member_object_null (const char*, bool = false);

      bool
      next_expect_member_object_null (const std::string&, bool = false);

      // Call next_expect_name() followed by next_expect(event::begin_array).
      // In the _null version also allow event::null, in which case return
      // false.
      //
      void
      next_expect_member_array (const char* name, bool skip_unknown = false);

      void
      next_expect_member_array (const std::string&, bool = false);

      bool
      next_expect_member_array_null (const char*, bool = false);

      bool
      next_expect_member_array_null (const std::string&, bool = false);

      // Get the next event and make sure it is the beginning of a value
      // (begin_object, begin_array, string, number, boolean, null). If it is
      // not, then throw invalid_json_input with appropriate description.
      // Otherwise, skip until the end of the value, recursively in case of
      // object and array.
      //
      // This function is primarily useful for skipping unknown object
      // members, for example:
      //
      //     while (p.next_expect (event::name, event::end_object))
      //     {
      //       if (p.name () == "known")
      //       {
      //         // Handle known member.
      //       }
      //       else
      //         p.next_expect_value_skip ();
      //     }
      //
      void
      next_expect_value_skip ();

      // Parsing location.
      //

      // Return the line number (1-based) corresponding to the most recently
      // parsed event or 0 if nothing has been parsed yet.
      //
      std::uint64_t
      line () const noexcept;

      // Return the column number (1-based) corresponding to the beginning of
      // the most recently parsed event or 0 if nothing has been parsed yet.
      //
      std::uint64_t
      column () const noexcept;

      // Return the position (byte offset) pointing immediately after the most
      // recently parsed event or 0 if nothing has been parsed yet.
      //
      std::uint64_t
      position () const noexcept;

      // Implementation details.
      //
    public:
      struct iterator
      {
        using value_type = event;

        explicit
        iterator (parser* p = nullptr, optional<event> e = nullopt)
            : p_ (p), e_ (e) {}

        event operator* () const {return *e_;}
        iterator& operator++ () {e_ = p_->next (); return *this;}

        // Comparison only makes sense when comparing to end (eof).
        //
        bool operator== (iterator y) const {return !e_ && !y.e_;}
        bool operator!= (iterator y) const {return !(*this == y);}

      private:
        parser* p_;
        optional<event> e_;
      };

      struct stream
      {
        std::istream*                is;
        optional<std::exception_ptr> exception;
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
      optional<event>
      translate (json_type) const noexcept;

      // Cache state (name/value) produced by the most recent call to
      // next_impl().
      //
      void
      cache_parsed_data ();

      // Cache the location numbers as determined by the most recent call to
      // next_impl().
      //
      void
      cache_parsed_location () noexcept;

      // Return true if this is a value event (string, number, boolean, or
      // null).
      //
      static bool
      value_event (optional<event>) noexcept;

      stream stream_;

      bool multi_value_;
      const char* separators_;

      // The *_p_ members indicate whether the value is present (cached).
      // Note: not using optional not to reallocate the string's buffer.
      //
      std::string name_;                       bool name_p_     = false;
      std::string value_;                      bool value_p_    = false;
      std::uint64_t line_, column_, position_; bool location_p_ = false;

      optional<json_type> parsed_; // Current parsed event if any.
      optional<json_type> peeked_; // Current peeked event if any.

      ::json_stream impl_[1];

      // Cached raw value.
      //
      const char* raw_s_;
      std::size_t raw_n_;
    };
  }
}

#include <libstud/json/parser.ixx>
