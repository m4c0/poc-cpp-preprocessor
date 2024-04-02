#pragma leco tool
#include <stdio.h>

import hai;
import missingno;
import traits;
import yoyo;

struct token {
  char c;
};

static hai::array<char> phase_1(const hai::array<char> &t) {
  hai::varray<char> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
static hai::array<char> phase_2(const hai::array<char> &t) {
  hai::varray<char> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
static hai::array<char> phase_3(const hai::array<char> &t) {
  hai::varray<char> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
static hai::array<char> phase_4(const hai::array<char> &t) {
  hai::varray<char> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}

static void print(const hai::array<char> &p) {
  printf("%*s", p.size(), p.begin());
}

static mno::req<void> preprocess_file(yoyo::reader &f) {
  return f.size()
      .map([](auto sz) { return hai::array<char>{sz}; })
      .fmap([&](auto &&buf) {
        return f.read(buf.begin(), buf.size())
            .map([&] { return phase_1(buf); })
            .map(phase_2)
            .map(phase_3)
            .map(phase_4)
            .map(print);
      });
}

static int print_error(const char *err) {
  fprintf(stderr, "%s", err);
  return 1;
}
static int success() { return 0; }

int main() {
  return yoyo::file_reader::open("poc.cpp") //
      .fmap(preprocess_file)
      .map(success)
      .take(print_error);
}
