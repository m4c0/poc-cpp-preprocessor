#pragma leco tool
#include <stdio.h>

import hai;
import missingno;
import traits;
import yoyo;

enum token_type : int { t_new_line = -1 };
struct token {
  token_type type;
  unsigned offset;
  unsigned len;
};

static hai::varray<token> phase_1(const hai::cstr &file) {
  hai::varray<token> res{file.size()};
  for (auto i = 0U; i < file.size(); i++) {
    switch (auto c = file.data()[i]) {
    case 0xd:
      if (file.data()[i + 1] == 0xa) {
        res.push_back(token{t_new_line, i, 2});
        i++;
      } else {
        res.push_back(token{t_new_line, i, 1});
      }
      break;
    default:
      // TODO: form translation charset elements
      // https://en.cppreference.com/w/cpp/language/charset#Translation_character_set
      res.push_back(token{static_cast<token_type>(c), i, 1});
    }
  }
  return res;
}

static hai::varray<token> phase_2(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
static hai::varray<token> phase_3(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
static hai::varray<token> phase_4(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}

static void print(const hai::varray<token> &tokens) {
  for (auto t : tokens) {
    switch (t.type) {
    case t_new_line:
      putc('\n', stdout);
      break;
    default:
      printf("%c", t.type);
    }
  }
}

static mno::req<void> preprocess_file(yoyo::reader &f) {
  return f.size()
      .map([](auto sz) { return hai::cstr{sz}; })
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
