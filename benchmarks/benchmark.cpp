#define __AVX__ 1

#include "absl/strings/charconv.h"
#include "absl/strings/numbers.h"
#include "fast_float/fast_float.h"
#include "./fast_float_new/fast_float.h"

#ifdef ENABLE_RYU
#include "ryu_parse.h"
#endif


#include "double-conversion/ieee.h"
#include "double-conversion/double-conversion.h"

#define IEEE_8087
#include "cxxopts.hpp"
#if defined(__linux__) || (__APPLE__ &&  __aarch64__)
#define USING_COUNTERS
#include "event_counter.h"
#endif
#include "dtoa.c"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>
#include <locale.h>

#include "random_generators.h"

/**
 * Determining whether we should import xlocale.h or not is
 * a bit of a nightmare.
 */
#ifdef __GLIBC__
#include <features.h>
#if !((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ > 25)))
#include <xlocale.h> // old glibc
#endif
#else            // not glibc
#ifndef _MSC_VER // assume that everything that is not GLIBC and not Visual
                 // Studio needs xlocale.h
#include <xlocale.h>
#endif
#endif

template <typename CharT>
double findmax_doubleconversion(std::vector<std::basic_string<CharT>> &s) {
  double answer = 0;
  double x;
  int flags = double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES |
              double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK |
              double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES;
  double empty_string_value = 0.0;
  uc16 separator = double_conversion::StringToDoubleConverter::kNoSeparator;
  double_conversion::StringToDoubleConverter converter(
      flags, empty_string_value, double_conversion::Double::NaN(), NULL, NULL,
      separator);
  int processed_characters_count;
  for (auto &st : s) {
    if constexpr (std::is_same<CharT, char16_t>::value) {
      x = converter.StringToDouble((const uc16*)st.data(), st.size(), &processed_characters_count);
    }
    else { x = converter.StringToDouble(st.data(), st.size(), &processed_characters_count); }
    
    if (processed_characters_count == 0) {
      throw std::runtime_error("bug in findmax_doubleconversion");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}

double findmax_netlib(std::vector<std::string> &s) {
  double answer = 0;
  double x = 0;
  for (std::string &st : s) {
    char *pr = (char *)st.data();
    x = netlib_strtod(st.data(), &pr);
    if (pr == st.data()) {
      throw std::runtime_error(std::string("bug in findmax_netlib ")+st);
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}

#ifdef ENABLE_RYU
double findmax_ryus2d(std::vector<std::string> &s) {
  double answer = 0;
  double x = 0;
  for (std::string &st : s) {
    // Ryu does not track character consumption (boo), but we can at least...
    Status stat = s2d(st.data(), &x);
    if (stat != SUCCESS) {
      throw std::runtime_error(std::string("bug in findmax_ryus2d ")+st + " " + std::to_string(stat));
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}
#endif

double findmax_strtod(std::vector<std::string> &s) {
  double answer = 0;
  double x = 0;
  for (std::string &st : s) {
    char *pr = (char *)st.data();
#ifdef _WIN32
    static _locale_t c_locale = _create_locale(LC_ALL, "C");
    x = _strtod_l(st.data(), &pr, c_locale);
#else
    static locale_t c_locale = newlocale(LC_ALL_MASK, "C", NULL);
    x = strtod_l(st.data(), &pr, c_locale);
#endif
    if (pr == st.data()) {
      throw std::runtime_error("bug in findmax_strtod");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}

#ifdef _WIN32
double findmax_strtod_16(std::vector<std::u16string>& s) {
  double answer = 0;
  double x = 0;
  for (auto& st : s) {
    auto* pr = (wchar_t*)st.data();
    static _locale_t c_locale = _create_locale(LC_ALL, "C");
    x = _wcstod_l((const wchar_t *)st.data(), &pr, c_locale);

    if (pr == (const wchar_t*)st.data()) {
      throw std::runtime_error("bug in findmax_strtod");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}
#endif


// Why not `|| __cplusplus > 201703L`? Because GNU libstdc++ does not have
// float parsing for std::from_chars.
#if defined(_MSC_VER)
#define FROM_CHARS_AVAILABLE_MAYBE
#endif

#ifdef FROM_CHARS_AVAILABLE_MAYBE
double findmax_from_chars(std::vector<std::string> &s) {
  double answer = 0;
  double x = 0;
  for (std::string &st : s) {
    auto [p, ec] = std::from_chars(st.data(), st.data() + st.size(), x);
    if (p == st.data()) {
      throw std::runtime_error("bug in findmax_from_chars");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}
#endif

template <typename CharT>
double findmax_fastfloat(std::vector<std::basic_string<CharT>> &s) {
  double answer = 0;
  double x = 0;
  for (auto &st : s) {
    auto [p, ec] = fast_float::from_chars(st.data(), st.data() + st.size(), x);
    if (p == st.data()) {
      throw std::runtime_error("bug in findmax_fastfloat");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}

template <typename CharT>
double findmax_fastfloat_new(std::vector<std::basic_string<CharT>>& s) {
  static_assert(fast_float_new::has_simd());
  double answer = 0;
  double x = 0;
  for (auto& st : s) {
    auto [p, ec] = fast_float_new::from_chars(st.data(), st.data() + st.size(), x);
    if (p == st.data()) {
      throw std::runtime_error("bug in findmax_fastfloat_new");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}

double findmax_absl_from_chars(std::vector<std::string> &s) {
  double answer = 0;
  double x = 0;
  for (std::string &st : s) {
    auto [p, ec] = absl::from_chars(st.data(), st.data() + st.size(), x);
    if (p == st.data()) {
      throw std::runtime_error("bug in findmax_absl_from_chars");
    }
    answer = answer > x ? answer : x;
  }
  return answer;
}
#ifdef USING_COUNTERS
template <class T, typename CharT>
std::vector<event_count> time_it_ns(std::vector<std::basic_string<CharT>> &lines,
                                     T const &function, size_t repeat) {
  std::vector<event_count> aggregate;
  event_collector collector;
  bool printed_bug = false;
  for (size_t i = 0; i < repeat; i++) {
    collector.start();
    double ts = function(lines);
    if (ts == 0 && !printed_bug) {
      printf("bug\n");
      printed_bug = true;
    }
    aggregate.push_back(collector.end());
 }
  return aggregate;
}

void pretty_print(double volume, size_t number_of_floats, std::string name, std::vector<event_count> events) {
  double volumeMB = volume / (1024. * 1024.);
  double average_ns{0};
  double min_ns{DBL_MAX};
  double cycles_min{DBL_MAX};
  double instructions_min{DBL_MAX};
  double cycles_avg{0};
  double instructions_avg{0};
  for(event_count e : events) {
    double ns = e.elapsed_ns();
    average_ns += ns;
    min_ns = min_ns < ns ? min_ns : ns;

    double cycles = e.cycles();
    cycles_avg += cycles;
    cycles_min = cycles_min < cycles ? cycles_min : cycles;

    double instructions = e.instructions();
    instructions_avg += instructions;
    instructions_min = instructions_min < instructions ? instructions_min : instructions;
  }
  cycles_avg /= events.size();
  instructions_avg /= events.size();
  average_ns /= events.size();
  printf("%-40s: %8.2f MB/s (+/- %.1f %%) ", name.data(),
           volumeMB * 1000000000 / min_ns,
           (average_ns - min_ns) * 100.0 / average_ns);
  printf("%8.2f Mfloat/s  ", 
           number_of_floats * 1000 / min_ns);
  if(instructions_min > 0) {
    printf(" %8.2f i/B %8.2f i/f (+/- %.1f %%) ", 
           instructions_min / volume,
           instructions_min / number_of_floats, 
           (instructions_avg - instructions_min) * 100.0 / instructions_avg);

    printf(" %8.2f c/B %8.2f c/f (+/- %.1f %%) ", 
           cycles_min / volume,
           cycles_min / number_of_floats, 
           (cycles_avg - cycles_min) * 100.0 / cycles_avg);
    printf(" %8.2f i/c ", 
           instructions_min /cycles_min);
    printf(" %8.2f GHz ", 
           cycles_min / min_ns);
  }
  printf("\n");

}
#else
template <class T, class CharT>
std::pair<double, double> time_it_ns(std::vector<std::basic_string<CharT>> &lines,
                                     T const &function, size_t repeat) {
  std::chrono::high_resolution_clock::time_point t1, t2;
  double average = 0;
  double min_value = DBL_MAX;
  bool printed_bug = false;
  for (size_t i = 0; i < repeat; i++) {
    t1 = std::chrono::high_resolution_clock::now();
    double ts = function(lines);
    if (ts == 0 && !printed_bug) {
      printf("bug\n");
      printed_bug = true;
    }
    t2 = std::chrono::high_resolution_clock::now();
    double dif =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    average += dif;
    min_value = min_value < dif ? min_value : dif;
  }
  average /= repeat;
  return std::make_pair(min_value, average);
}




void pretty_print(double volume, size_t number_of_floats, std::string name, std::pair<double,double> result) {
  double volumeMB = volume / (1024. * 1024.);
  printf("%-40s: %8.2f MB/s (+/- %.1f %%) ", name.data(),
           volumeMB * 1000000000 / result.first,
           (result.second - result.first) * 100.0 / result.second);
  printf("%8.2f Mfloat/s  ", 
           number_of_floats * 1000 / result.first);
  printf(" %8.2f ns/f \n", 
           double(result.first) /number_of_floats );
}
#endif 
void process(std::vector<std::string> &lines, size_t volume) {
  size_t repeat = 100;
  double volumeMB = volume / (1024. * 1024.);
  std::cout << "volume = " << volumeMB << " MB " << std::endl;
  std::cout << "char_type = char" << std::endl;
  pretty_print(volume, lines.size(), "netlib", time_it_ns(lines, findmax_netlib, repeat));
  pretty_print(volume, lines.size(), "doubleconversion", time_it_ns(lines, findmax_doubleconversion<char>, repeat));
  pretty_print(volume, lines.size(), "strtod", time_it_ns(lines, findmax_strtod, repeat));
#ifdef ENABLE_RYU
  pretty_print(volume, lines.size(), "ryu_parse", time_it_ns(lines, findmax_ryus2d, repeat));
#endif
  pretty_print(volume, lines.size(), "abseil", time_it_ns(lines, findmax_absl_from_chars, repeat));  
  
  pretty_print(volume, lines.size(), "fastfloat", time_it_ns(lines, findmax_fastfloat<char>, repeat));
  pretty_print(volume, lines.size(), "fastfloat_simd", time_it_ns(lines, findmax_fastfloat_new<char>, repeat));
  //pretty_print(volume, lines.size(), "fastfloat_new2", time_it_ns(lines, findmax_fastfloat_new2<char>, repeat));
  
#ifdef FROM_CHARS_AVAILABLE_MAYBE
  pretty_print(volume, lines.size(), "from_chars", time_it_ns(lines, findmax_from_chars, repeat));
#endif
}

void process16(std::vector<std::u16string>& lines, size_t volume) {
  size_t repeat = 100;
  volume *= 2; // 2 bytes per char
  double volumeMB = volume / (1024. * 1024.);
  std::cout << "\n\nvolume = " << volumeMB << " MB " << std::endl;
  std::cout << "char_type = char16_t" << std::endl;
  pretty_print(volume, lines.size(), "doubleconversion", time_it_ns(lines, findmax_doubleconversion<char16_t>, repeat));
#ifdef _WIN32
  pretty_print(volume, lines.size(), "wcstod", time_it_ns(lines, findmax_strtod_16, repeat));
#endif
#ifdef ENABLE_RYU
  pretty_print(volume, lines.size(), "ryu_parse", time_it_ns(lines, findmax_ryus2d, repeat));
#endif
  //pretty_print(volume, lines.size(), "abseil", time_it_ns(lines, findmax_absl_from_chars, repeat)); 
  
  pretty_print(volume, lines.size(), "fastfloat", time_it_ns(lines, findmax_fastfloat<char16_t>, repeat));  
  pretty_print(volume, lines.size(), "fastfloat_simd", time_it_ns(lines, findmax_fastfloat_new<char16_t>, repeat));
  //pretty_print(volumeMB, lines.size(), "fastfloat_new2", time_it_ns(lines, findmax_fastfloat_new2<char16_t>, repeat));
#ifdef FROM_CHARS_AVAILABLE_MAYBE
  //pretty_print(volume, lines.size(), "from_chars", time_it_ns(lines, findmax_from_chars, repeat));
#endif
}

// this is okay, all chars are ASCII
inline std::u16string widen(std::string line)
{
  std::u16string u16line;
  u16line.resize(line.size());
  for (size_t i = 0; i < line.size(); ++i)
    u16line[i] = char16_t(line[i]);
  return u16line;
}

void fileload(const char *filename) {
  std::ifstream inputfile(filename);
  if (!inputfile) {
    std::cerr << "can't open " << filename << std::endl;
    return;
  }
  std::string line;
  std::vector<std::string> lines;
  std::vector<std::u16string> u16lines;
  
  lines.reserve(10000); // let us reserve plenty of memory.
  u16lines.reserve(10000);

  size_t volume = 0;
  while (getline(inputfile, line)) {
    volume += line.size();
    lines.push_back(line);
    u16lines.push_back(widen(line));
  }

  std::cout << "# read " << lines.size() << " lines " << std::endl;
  process(lines, volume);
  process16(u16lines, volume);
}


void parse_random_numbers(size_t howmany, bool concise, std::string random_model) {
  std::cout << "# parsing random numbers" << std::endl;
  std::vector<std::string> lines;
  std::vector<std::u16string> u16lines;
  auto g = std::unique_ptr<string_number_generator>(get_generator_by_name(random_model));
  std::cout << "model: " << g->describe() << std::endl;
  if(concise) { std::cout << "concise (using as few digits as possible)"  << std::endl; }
  std::cout << "volume: "<< howmany << " floats"  << std::endl;

  lines.reserve(howmany); // let us reserve plenty of memory.
  u16lines.reserve(howmany); // let us reserve plenty of memory.

  size_t volume = 0;
  for (size_t i = 0; i < howmany; i++) {
    std::string line =  g->new_string(concise);
    volume += line.size();
    lines.push_back(line);
    u16lines.push_back(widen(line));
  } 
  process(lines, volume);
  process16(u16lines, volume);
  
}

void parse_contrived(size_t howmany, const char *filename) {
  std::cout << "# parsing contrived numbers" << std::endl;
  std::cout << "# these are contrived test cases to test specific algorithms" << std::endl;
  std::vector<std::string> lines;
  std::vector<std::u16string> u16lines;
  std::cout << "volume: "<< howmany << " floats"  << std::endl;

  std::ifstream inputfile(filename);
  if (!inputfile) {
    std::cerr << "can't open " << filename << std::endl;
    return;
  }
  lines.reserve(howmany); // let us reserve plenty of memory.
  u16lines.reserve(howmany);
  std::string line;
  while (getline(inputfile, line)) {
    std::cout << "testing contrived case: \"" << line << "\"" << std::endl;
    size_t volume = 0;
    for (size_t i = 0; i < howmany; i++) {
      lines.push_back(line);
      u16lines.push_back(widen(line));
    }
    process(lines, volume);
    process16(u16lines, volume);
    lines.clear();
    u16lines.clear();
    std::cout << "-----------------------" << std::endl;
  }
}

cxxopts::Options
    options("benchmark",
            "Compute the parsing speed of different number parsers.");

int main(int argc, char **argv) {
  try {
    options.add_options()
        ("c,concise", "Concise random floating-point strings (if not 17 digits are used)")
        ("f,file", "File name.", cxxopts::value<std::string>()->default_value(""))
        ("v,volume", "Volume (number of floats generated).", cxxopts::value<size_t>()->default_value("100000"))
        ("m,model", "Random Model.", cxxopts::value<std::string>()->default_value("uniform"))
        ("h,help","Print usage.");
    auto result = options.parse(argc, argv);
    if(result["help"].as<bool>()) {
      std::cout << options.help() << std::endl;
      return EXIT_SUCCESS;
    }
    auto filename = result["file"].as<std::string>();
    if (filename.find("contrived") != std::string::npos) {
      parse_contrived(result["volume"].as<size_t>(), filename.c_str());
      std::cout << "# You can also provide a filename (with the -f flag): it should contain one "
                   "string per line corresponding to a number"
                << std::endl;
    } else if (filename.empty()) {
      parse_random_numbers(result["volume"].as<size_t>(), result["concise"].as<bool>(), result["model"].as<std::string>());
      std::cout << "# You can also provide a filename (with the -f flag): it should contain one "
                   "string per line corresponding to a number"
                << std::endl;
    } else {
      fileload(filename.c_str());
    }
  } catch (const cxxopts::OptionException &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
