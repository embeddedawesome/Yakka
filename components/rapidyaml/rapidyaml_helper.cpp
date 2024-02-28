// ryml can be used as a single header, or as a simple library:
#if defined(RYML_SINGLE_HEADER) // using the single header directly in the executable
#define RYML_SINGLE_HDR_DEFINE_NOW
#include <ryml_all.hpp>
#elif defined(RYML_SINGLE_HEADER_LIB) // using the single header from a library
#include <ryml_all.hpp>
#else
#include <ryml.hpp>
// <ryml_std.hpp> is needed if interop with std containers is
// desired; ryml itself does not use any STL container.
// For this sample, we will be using std interop, so...
#include <ryml_std.hpp>  // optional header, provided for std:: interop
#include <c4/format.hpp> // needed for the examples below
#endif

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4996) // fopen: this function or variable may be unsafe
/** load a file from disk and return a newly created CharContainer */
template <class CharContainer> size_t file_get_contents(std::string filename, CharContainer *v)
{
  ::FILE *fp = ::fopen(filename.c_str(), "rb");
  C4_CHECK_MSG(fp != nullptr, "could not open file");
  ::fseek(fp, 0, SEEK_END);
  long sz = ::ftell(fp);
  v->resize(static_cast<typename CharContainer::size_type>(sz));
  if (sz) {
    ::rewind(fp);
    size_t ret = ::fread(&(*v)[0], 1, v->size(), fp);
    C4_CHECK(ret == (size_t)sz);
  }
  ::fclose(fp);
  return v->size();
}

/** load a file from disk into an existing CharContainer */
template <class CharContainer> CharContainer file_get_contents(std::string filename)
{
  CharContainer cc;
  file_get_contents(filename, &cc);
  return cc;
}

ryml::Tree &parse_file(std::string filename)
{
  std::string contents = file_get_contents<std::string>(filename);
  ryml::Tree tree      = ryml::parse_in_arena(ryml::to_csubstr(contents)); // immutable (csubstr) overload
  return tree;
}