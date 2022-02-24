namespace stud
{
  namespace json
  {
    inline invalid_json_output::
    invalid_json_output (optional<event_type> e,
                         error_code c,
                         const char* d,
                         std::size_t o)
        : std::invalid_argument (d), event (e), code (c), offset (o)
    {
    }

    inline invalid_json_output::
    invalid_json_output (optional<event_type> e,
                         error_code c,
                         const std::string& d,
                         std::size_t o)
        : invalid_json_output (e, c, d.c_str (), o)
    {
    }

    inline buffer_serializer::
    buffer_serializer (void* b, std::size_t& s, std::size_t c,
                       overflow_function* o, flush_function* f, void* d,
                       std::size_t i)
        : buf_ {b, s, c},
          overflow_ (o),
          flush_ (f),
          data_ (d),
          indent_ (i),
          sep_ (indent_ != 0 ? ",\n" : "")
    {
    }

    template <std::size_t N>
    inline buffer_serializer::
    buffer_serializer (std::array<char, N>& a, std::size_t& s, std::size_t i)
        : buffer_serializer (a.data (), s, a.size (),
                             nullptr, nullptr, nullptr,
                             i)
    {
    }

    inline buffer_serializer::
    buffer_serializer (void* b, std::size_t& s, std::size_t c, std::size_t i)
        : buffer_serializer (b, s, c, nullptr, nullptr, nullptr, i)
    {
    }

    inline buffer_serializer::
    buffer_serializer (void* b, std::size_t c,
                       overflow_function* o, flush_function* f, void* d,
                       std::size_t i)
        : buffer_serializer (b, size_, c, o, f, d, i)
    {
      size_ = 0;
    }

    inline void buffer_serializer::
    begin_object ()
    {
      next (event::begin_object);
    }

    inline void buffer_serializer::
    end_object ()
    {
      next (event::end_object);
    }

    inline void buffer_serializer::
    member_name (const char* n, bool c)
    {
      next (event::name, {n, n != nullptr ? strlen (n) : 0}, c);
    }

    inline void buffer_serializer::
    member_name (const std::string& n, bool c)
    {
      next (event::name, {n.c_str (), n.size ()}, c);
    }

    template <typename T>
    inline void buffer_serializer::
    member (const char* n, const T& v, bool c)
    {
      member_name (n, c);
      value (v, c);
    }

    template <typename T>
    inline void buffer_serializer::
    member (const std::string& n, const T& v, bool c)
    {
      member_name (n, c);
      value (v, c);
    }

    inline void buffer_serializer::
    begin_array ()
    {
      next (event::begin_array);
    }

    inline void buffer_serializer::
    end_array ()
    {
      next (event::end_array);
    }

    inline void buffer_serializer::
    value (const char* v, bool c)
    {
      if (v != nullptr)
        next (event::string, {v, strlen (v)}, c);
      else
        next (event::null);
    }

    inline void buffer_serializer::
    value (const std::string& v, bool c)
    {
      next (event::string, {v.c_str (), v.size ()}, c);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value ||
                            std::is_floating_point<T>::value>::type
    buffer_serializer::
    value (T v)
    {
      // The largest 128-bit integer has 39 digits, and long floating point
      // numbers will fit because they are output in scientific notation.
      //
      char b[40];
      const std::size_t n (to_chars (b, sizeof (b), v));
      next (event::number, {b, n});
    }

    inline void buffer_serializer::
    value (bool b)
    {
      next (event::boolean,
            b ? std::make_pair ("true", 4) : std::make_pair ("false", 5));
    }

    inline void buffer_serializer::
    value (std::nullptr_t)
    {
      next (event::null);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, int v)
    {
      return to_chars_impl (b, s, "%d", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, long v)
    {
      return to_chars_impl (b, s, "%ld", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, long long v)
    {
      return to_chars_impl (b, s, "%lld", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, unsigned v)
    {
      return to_chars_impl (b, s, "%u", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, unsigned long v)
    {
      return to_chars_impl (b, s, "%lu", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, unsigned long long v)
    {
      return to_chars_impl (b, s, "%llu", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, double v)
    {
      return to_chars_impl (b, s, "%.10g", v);
    }

    inline size_t buffer_serializer::
    to_chars (char* b, size_t s, long double v)
    {
      return to_chars_impl (b, s, "%.10Lg", v);
    }
  }
}
