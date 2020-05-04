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
    inline invalid_json::
    invalid_json (std::string n,
                  std::uint64_t l,
                  std::uint64_t c,
                  std::uint64_t p,
                  const std::string& d)
        : invalid_json (move (n), l, c, p, d.c_str ())
    {
    }

    inline invalid_json::
    invalid_json (std::string n,
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
    parser (std::istream& is, const std::string& n, bool mv, const char* sep)
        : parser (is, n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const void* t,
            std::size_t s,
            const std::string& n,
            bool mv,
            const char* sep)
        : parser (t, s, n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const std::string& t,
            const std::string& n,
            bool mv,
            const char* sep)
        : parser (t.data (), t.size (), n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const std::string& t,
            const char* n,
            bool mv,
            const char* sep)
        : parser (t.data (), t.size (), n, mv, sep)
    {
    }

    inline parser::
    parser (const char* t,
            const std::string& n,
            bool mv,
            const char* sep)
        : parser (t, std::strlen (t), n.c_str (), mv, sep)
    {
    }

    inline parser::
    parser (const char* t,
            const char* n,
            bool mv,
            const char* sep)
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
  }
}
