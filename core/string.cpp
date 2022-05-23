#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

using ::std::string;

#pragma warning(disable : 4996)

#ifndef va_copy
#ifdef _MSC_VER
#define va_copy(dst, src) dst=src
#elif !(__cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__))
#define va_copy(dst, src) memcpy((void*)dst, (void*)src, sizeof(*src))
#endif
#endif

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format of message
/// \param ap Variable argument list
///
void toString(string &dst, const char *format, va_list ap) throw() {
  int length;
  va_list apStrLen;
  va_copy(apStrLen, ap);
  length = vsnprintf(NULL, 0, format, apStrLen);
  va_end(apStrLen);
  if (length > 0) {
    dst.resize(length);
    vsnprintf((char *)dst.data(), dst.size() + 1, format, ap);
  } else {
    dst = "Format error! format: ";
    dst.append(format);
  }
}

///
/// \breif Format message
/// \param dst String to store formatted message
/// \param format Format of message
/// \param ... Variable argument list
///
void toString(string &dst, const char *format, ...) throw() {
  va_list ap;
  va_start(ap, format);
  toString(dst, format, ap);
  va_end(ap);
}

///
/// \breif Format message
/// \param format Format of message
/// \param ... Variable argument list
///
string toString(const char *format, ...) throw() {
  string dst;
  va_list ap;
  va_start(ap, format);
  toString(dst, format, ap);
  va_end(ap);
  return dst;
}

///
/// \breif Format message
/// \param format Format of message
/// \param ap Variable argument list
///
string toString(const char *format, va_list ap) throw() {
  string dst;
  toString(dst, format, ap);
  return dst;
}

/*
int main() {
  int a = 32;
  const char * str = "This works!";

  string test(toString("\nSome testing: a = %d, %s\n", a, str));
  printf(test.c_str());

  a = 0x7fffffff;
  test = toString("\nMore testing: a = %d, %s\n", a, "This works too..");
  printf(test.c_str());

  a = 0x80000000;
  toString(test, "\nMore testing: a = %d, %s\n", a, "This way is cheaper");
  printf(test.c_str());

  return 0;
}
*/
