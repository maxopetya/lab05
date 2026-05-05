#include <fstream>
#include <sstream>

#include <print.hpp>

#include <gtest/gtest.h>

TEST(Print, InFileStream) {
  std::string filepath = "file.txt";
  std::string text = "hello";
  std::ofstream out{filepath};

  print(text, out);
  out.close();

  std::string result;
  std::ifstream in{filepath};
  in >> result;

  EXPECT_EQ(result, text);
}

TEST(Print, InOstream) {
  std::ostringstream out;
  std::string text = "world";
  print(text, out);
  EXPECT_EQ(out.str(), text);
}
