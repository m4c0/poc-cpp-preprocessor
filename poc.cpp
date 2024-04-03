#pragma leco tool
#include <stdio.h>

import hai;
import missingno;
import traits;
import yoyo;

enum token_type : int {
  t_null = -1,
  t_new_line = '\n',
  t_space = ' ',
};
struct token {
  token_type type;
  unsigned begin;
  unsigned end;
};

static hai::varray<token> phase_1(const hai::cstr &file) {
  hai::varray<token> res{file.size()};
  for (auto i = 0U; i < file.size(); i++) {
    switch (auto c = file.data()[i]) {
    case 0xd:
      if (file.data()[i + 1] == 0xa) {
        res.push_back(token{.type = t_new_line, .begin = i, .end = i + 1});
        i++;
      } else {
        res.push_back(token{.type = t_new_line, .begin = i, .end = i});
      }
      break;
    default:
      // TODO: form translation charset elements
      // https://en.cppreference.com/w/cpp/language/charset#Translation_character_set
      res.push_back(token{
          .type = static_cast<token_type>(c),
          .begin = i,
          .end = i,
      });
    }
  }
  return res;
}

static hai::varray<token> phase_2(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};

  auto start = 0U;
  if (t.size() >= 2 && t[0].type == '0xFE' && t[1].type == '0xFF')
    start = 2U;

  for (auto i = start; i < t.size(); i++) {
    if (t[i].type != '\\') {
      res.push_back(t[i]);
      continue;
    }
    token nt{t[i]};
    for (auto j = i + 1; j < t.size(); j++) {
      if (t[j].type == t_new_line) {
        nt.type = t_null;
        i = j + 1;
        break;
      }
      if (t[j].type != ' ' && t[j].type != '\t') {
        break;
      }
    }
    if (nt.type != t_null)
      res.push_back(nt);
  }

  if (res.size() > 0 && res[res.size() - 1].type != t_new_line) {
    const auto &last = res[res.size() - 1];
    res.push_back(token{
        .type = t_new_line,
        .begin = last.end + 1,
        .end = last.end + 1,
    });
  }
  return res;
}

static token comment(const hai::varray<token> &t, unsigned offset) {
  auto t1 = t[offset + 1];
  if (t1.type == '*') {
    return token{.type = t_null};
  } else if (t1.type == '/') {
    return token{.type = t_null};
  } else {
    return t[offset];
  }
}

static hai::varray<token> phase_3(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  for (auto i = 0U; i < t.size(); i++) {
    token nt = t[i];
    if (nt.type == '/') {
      nt = comment(t, i);
    }
    if (nt.type != t_null)
      res.push_back(nt);
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
    putc(t.type, stdout);
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
  return yoyo::file_reader::open("example.cpp") //
      .fmap(preprocess_file)
      .map(success)
      .take(print_error);
}
