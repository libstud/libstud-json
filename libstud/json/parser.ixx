#include <cerrno>
#include <limits>      // numeric_limits
#include <utility>     // move()
#include <cstdlib>     // strto*()
#include <type_traits> // enable_if, is_*

namespace stud
{
  namespace json
  {
    inline invalid_json::
    invalid_json (std::string n,
                  std::uint64_t l,
                  std::uint64_t c,
                  const std::string& d)
        : invalid_json (move (n), l, c, d.c_str ())
    {
    }

    inline invalid_json::
    invalid_json (std::string n,
                  std::uint64_t l,
                  std::uint64_t c,
                  const char* d)
        : invalid_argument (d),
          name (std::move (n)),
          line (l), column (c)
    {
    }

    inline parser::
    parser (std::istream& is, const std::string& n)
        : parser (is, n.c_str ())
    {
    }

    inline parser::
    parser (const void* t, std::size_t s, const std::string& n)
        : parser (t, s, n.c_str ())
    {
    }

    inline const std::string& parser::
    name ()
    {
      name_.assign (raw_s_, raw_n_);
      return name_;
    }

    inline std::string& parser::
    value ()
    {
      value_.assign (raw_s_, raw_n_);
      return value_;
    }

    // Note: one day we will be able to use C++17 from_chars() which was made
    // exactly for this.
    //
    template <typename T>
    inline typename std::enable_if<std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t, parser&)
    {
      return *b == 't';
    }

    template <typename T>
    inline typename std::enable_if<
      std::is_integral<T>::value &&
      std::is_signed<T>::value &&
      !std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t n, parser& p)
    {
      char* e (nullptr);
      std::int64_t v (strtoll (b, &e, 10)); // Can't throw.

      if (e == b || e != b + n || errno == ERANGE ||
          v < std::numeric_limits<T>::min () ||
          v > std::numeric_limits<T>::max ())
        p.throw_invalid_value ("signed integer");

      return static_cast<T> (v);
    }

    template <typename T>
    inline typename std::enable_if<
      std::is_integral<T>::value &&
      std::is_unsigned<T>::value &&
      !std::is_same<T, bool>::value, T>::type
    parse_value (const char* b, size_t n, parser& p)
    {
      char* e (nullptr);
      std::uint64_t v (strtoull (b, &e, 10)); // Can't throw.

      if (e == b || e != b + n || errno == ERANGE ||
          v > std::numeric_limits<T>::max ())
        p.throw_invalid_value ("unsigned integer");

      return static_cast<T> (v);
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, float>::value, T>::type
    parse_value (const char* b, size_t n, parser& p)
    {
      char* e (nullptr);
      T r (std::strtof (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("float");

      return r;
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, double>::value, T>::type
    parse_value (const char* b, size_t n, parser& p)
    {
      char* e (nullptr);
      T r (std::strtod (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("double");

      return r;
    }

    template <typename T>
    inline typename std::enable_if<std::is_same<T, long double>::value, T>::type
    parse_value (const char* b, size_t n, parser& p)
    {
      char* e (nullptr);
      T r (std::strtold (b, &e));

      if (e == b || e != b + n || errno == ERANGE)
        p.throw_invalid_value ("long double");

      return r;
    }

    template <typename T>
    inline T parser::
    value ()
    {
      return parse_value<T> (raw_s_, raw_n_, *this);
    }
  }
}
