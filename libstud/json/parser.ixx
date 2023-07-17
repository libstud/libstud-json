#include <cerrno>
#include <limits>      // numeric_limits
#include <utility>     // move()
#include <cassert>
#include <cstdlib>     // strto*()
#include <type_traits> // enable_if, is_*
#include <cstring>     // strlen()

namespace stud
{
  namespace json
  {
    inline invalid_json_input::
    invalid_json_input (std::string n,
                        std::uint64_t l,
                        std::uint64_t c,
                        std::uint64_t p,
                        const std::string& d)
        : invalid_json_input (move (n), l, c, p, d.c_str ())
    {
    }

    inline invalid_json_input::
    invalid_json_input (std::string n,
                        std::uint64_t l,
                        std::uint64_t c,
                        std::uint64_t p,
                        const char* d)
        : invalid_argument (d),
          name (std::move (n)),
          line (l), column (c), position (p)
    {
    }

    inline parser::
    parser (std::istream& is,
            const std::string& n,
            bool mv,
            const char* sep) noexcept
        : parser (is, n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const void* t,
            std::size_t s,
            const std::string& n,
            bool mv,
            const char* sep) noexcept
        : parser (t, s, n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const std::string& t,
            const std::string& n,
            bool mv,
            const char* sep) noexcept
        : parser (t.data (), t.size (), n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const std::string& t,
            const char* n,
            bool mv,
            const char* sep) noexcept
        : parser (t.data (), t.size (), n, mv, sep)
    {
    }

    inline parser::
    parser (const char* t,
            const std::string& n,
            bool mv,
            const char* sep) noexcept
        : parser (t, std::strlen (t), n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const char* t,
            const char* n,
            bool mv,
            const char* sep) noexcept
        : parser (t, std::strlen (t), n, mv, sep)
    {
    }

    inline const std::string& parser::
    name ()
    {
      if (!name_p_)
      {
        assert (parsed_ && !peeked_ && !value_p_);
        cache_parsed_data ();
        assert (name_p_);
      }
      return name_;
    }

    inline std::string& parser::
    value ()
    {
      if (!value_p_)
      {
        assert (parsed_ && !peeked_ && !name_p_);
        cache_parsed_data ();
        assert (value_p_);
      }
      return value_;
    }

    // Note: one day we will be able to use C++17 from_chars() which was made
    // exactly for this.
    //
    template <typename T>
    inline typename std::enable_if<std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t, const parser&)
    {
      return *b == 't';
    }

    template <typename T>
    inline typename std::enable_if<
      std::is_integral<T>::value &&
      std::is_signed<T>::value &&
      !std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t n, const parser& p)
    {
      char* e (nullptr);
      errno = 0; // We must clear it according to POSIX.
      std::int64_t v (strtoll (b, &e, 10)); // Can't throw.

      if (e == b || e != b + n || errno == ERANGE ||
          v < std::numeric_limits<T>::min () ||
          v > std::numeric_limits<T>::max ())
        p.throw_invalid_value ("signed integer", b, n);

      return static_cast<T> (v);
    }

    template <typename T>
    inline typename std::enable_if<
      std::is_integral<T>::value &&
      std::is_unsigned<T>::value &&
      !std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t n, const parser& p)
    {
      char* e (nullptr);
      errno = 0; // We must clear it according to POSIX.
      std::uint64_t v (strtoull (b, &e, 10)); // Can't throw.

      if (e == b || e != b + n || errno == ERANGE ||
          v > std::numeric_limits<T>::max ())
        p.throw_invalid_value ("unsigned integer", b, n);

      return static_cast<T> (v);
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, float>::value, T>::type
    parse_value (const char* b, size_t n, const parser& p)
    {
      char* e (nullptr);
      errno = 0; // We must clear it according to POSIX.
      T r (std::strtof (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("float", b, n);

      return r;
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, double>::value, T>::type
    parse_value (const char* b, size_t n, const parser& p)
    {
      char* e (nullptr);
      errno = 0; // We must clear it according to POSIX.
      T r (std::strtod (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("double", b, n);

      return r;
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, long double>::value, T>::type
    parse_value (const char* b, size_t n, const parser& p)
    {
      char* e (nullptr);
      errno = 0; // We must clear it according to POSIX.
      T r (std::strtold (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("long double", b, n);

      return r;
    }

    template <typename T>
    inline T parser::
    value () const
    {
      if (!value_p_)
      {
        assert (parsed_ && !peeked_ && value_event (translate (*parsed_)));
        return parse_value<T> (raw_s_, raw_n_, *this);
      }

      return parse_value<T> (value_.data (), value_.size (), *this);
    }

    inline void parser::
    next_expect_name (const std::string& n, bool su)
    {
      next_expect_name (n.c_str (), su);
    }

    // next_expect_<type>()
    //
    inline std::string& parser::
    next_expect_string ()
    {
      next_expect (event::string);
      return value ();
    }

    template <typename T>
    inline T parser::
    next_expect_string ()
    {
      next_expect (event::string);
      return value<T> ();
    }

    inline std::string& parser::
    next_expect_number ()
    {
      next_expect (event::number);
      return value ();
    }

    template <typename T>
    inline T parser::
    next_expect_number ()
    {
      next_expect (event::number);
      return value<T> ();
    }

    inline std::string& parser::
    next_expect_boolean ()
    {
      next_expect (event::boolean);
      return value ();
    }

    template <typename T>
    inline T parser::
    next_expect_boolean ()
    {
      next_expect (event::boolean);
      return value<T> ();
    }

    // next_expect_<type>_null()
    //
    inline std::string* parser::
    next_expect_string_null ()
    {
      return next_expect (event::string, event::null) ? &value () : nullptr;
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_string_null ()
    {
      return next_expect (event::string, event::null)
        ? optional<T> (value<T> ())
        : nullopt;
    }

    inline std::string* parser::
    next_expect_number_null ()
    {
      return next_expect (event::number, event::null) ? &value () : nullptr;
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_number_null ()
    {
      return next_expect (event::number, event::null)
        ? optional<T> (value<T> ())
        : nullopt;
    }

    inline std::string* parser::
    next_expect_boolean_null ()
    {
      return next_expect (event::boolean, event::null) ? &value () : nullptr;
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_boolean_null ()
    {
      return next_expect (event::boolean, event::null)
        ? optional<T> (value<T> ())
        : nullopt;
    }

    // next_expect_member_string()
    //
    inline std::string& parser::
    next_expect_member_string (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_string ();
    }

    inline std::string& parser::
    next_expect_member_string (const std::string& n, bool su)
    {
      return next_expect_member_string (n.c_str (), su);
    }

    template <typename T>
    inline T parser::
    next_expect_member_string (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_string<T> ();
    }

    template <typename T>
    inline T parser::
    next_expect_member_string (const std::string& n, bool su)
    {
      return next_expect_member_string<T> (n.c_str (), su);
    }

    // next_expect_member_number()
    //
    inline std::string& parser::
    next_expect_member_number (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_number ();
    }

    inline std::string& parser::
    next_expect_member_number (const std::string& n, bool su)
    {
      return next_expect_member_number (n.c_str (), su);
    }

    template <typename T>
    inline T parser::
    next_expect_member_number (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_number<T> ();
    }

    template <typename T>
    inline T parser::
    next_expect_member_number (const std::string& n, bool su)
    {
      return next_expect_member_number<T> (n.c_str (), su);
    }

    // next_expect_member_boolean()
    //
    inline std::string& parser::
    next_expect_member_boolean (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_boolean ();
    }

    inline std::string& parser::
    next_expect_member_boolean (const std::string& n, bool su)
    {
      return next_expect_member_boolean (n.c_str (), su);
    }

    template <typename T>
    inline T parser::
    next_expect_member_boolean (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_boolean<T> ();
    }

    template <typename T>
    inline T parser::
    next_expect_member_boolean (const std::string& n, bool su)
    {
      return next_expect_member_boolean<T> (n.c_str (), su);
    }

    // next_expect_member_string_null()
    //
    inline std::string* parser::
    next_expect_member_string_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_string_null ();
    }

    inline std::string* parser::
    next_expect_member_string_null (const std::string& n, bool su)
    {
      return next_expect_member_string_null (n.c_str (), su);
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_string_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_string_null<T> ();
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_string_null (const std::string& n, bool su)
    {
      return next_expect_member_string_null<T> (n.c_str (), su);
    }

    // next_expect_member_number_null()
    //
    inline std::string* parser::
    next_expect_member_number_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_number_null ();
    }

    inline std::string* parser::
    next_expect_member_number_null (const std::string& n, bool su)
    {
      return next_expect_member_number_null (n.c_str (), su);
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_number_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_number_null<T> ();
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_number_null (const std::string& n, bool su)
    {
      return next_expect_member_number_null<T> (n.c_str (), su);
    }

    // next_expect_member_boolean_null()
    //
    inline std::string* parser::
    next_expect_member_boolean_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_boolean_null ();
    }

    inline std::string* parser::
    next_expect_member_boolean_null (const std::string& n, bool su)
    {
      return next_expect_member_boolean_null (n.c_str (), su);
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_boolean_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect_boolean_null<T> ();
    }

    template <typename T>
    inline optional<T> parser::
    next_expect_member_boolean_null (const std::string& n, bool su)
    {
      return next_expect_member_boolean_null<T> (n.c_str (), su);
    }

    // next_expect_member_object[_null]()
    //
    inline void parser::
    next_expect_member_object (const char* n, bool su)
    {
      next_expect_name (n, su);
      next_expect (event::begin_object);
    }

    inline void parser::
    next_expect_member_object (const std::string& n, bool su)
    {
      next_expect_member_object (n.c_str (), su);
    }

    inline bool parser::
    next_expect_member_object_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect (event::begin_object, event::null);
    }

    inline bool parser::
    next_expect_member_object_null (const std::string& n, bool su)
    {
      return next_expect_member_object_null (n.c_str (), su);
    }

    // next_expect_member_array[_null]()
    //
    inline void parser::
    next_expect_member_array (const char* n, bool su)
    {
      next_expect_name (n, su);
      next_expect (event::begin_array);
    }

    inline void parser::
    next_expect_member_array (const std::string& n, bool su)
    {
      next_expect_member_array (n.c_str (), su);
    }

    inline bool parser::
    next_expect_member_array_null (const char* n, bool su)
    {
      next_expect_name (n, su);
      return next_expect (event::begin_array, event::null);
    }

    inline bool parser::
    next_expect_member_array_null (const std::string& n, bool su)
    {
      return next_expect_member_array_null (n.c_str (), su);
    }
  }
}
