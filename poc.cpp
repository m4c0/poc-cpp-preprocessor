#pragma leco tool
#include <stdio.h>

import hai;
import missingno;
import traits;
import yoyo;

enum token_type : int {
  t_eof = -2,
  t_null = -1,
  t_new_line = '\n',
  t_space = ' ',
};
struct token {
  token_type type;
  unsigned begin;
  unsigned end;
};

// {{{ Token Stream
class token_stream {
  const hai::varray<token> &m_tokens;
  unsigned offset{};

  token eof() const {
    const auto &last = m_tokens[m_tokens.size() - 1];
    return token{.type = t_eof, .begin = last.end + 1, .end = last.end + 1};
  }

public:
  explicit token_stream(const hai::varray<token> &t) : m_tokens(t) {}

  bool has_more() { return offset < m_tokens.size(); }

  void skip(unsigned n) {
    offset = (offset + n >= m_tokens.size()) ? m_tokens.size() : offset + n;
  }
  [[nodiscard]] token take() {
    if (offset >= m_tokens.size())
      return eof();
    return m_tokens[offset++];
  }
  [[nodiscard]] token peek(unsigned d = 0) const {
    if (offset + d >= m_tokens.size())
      return eof();
    return m_tokens[offset + d];
  }
};
// }}}

// {{{ Phase 1
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
// }}}

// {{{ Phase 2
static hai::varray<token> phase_2(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  token_stream str{t};

  if (str.peek(0).type == 0xFE && str.peek(1).type == 0xFF) {
    str.skip(2);
  }

  while (str.has_more()) {
    token t = str.take();
    if (t.type != '\\') {
      res.push_back(t);
      continue;
    }
    unsigned la = 0;
    while (str.peek(la).type != t_eof) {
      auto nt = str.peek(la);
      if (nt.type == t_new_line)
        break;
      if (nt.type != ' ' && nt.type != '\t')
        break;
      la++;
    }
    if (str.peek(la).type == t_new_line) {
      str.skip(la + 1);
    } else {
      res.push_back(t);
    }
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
// }}}

// {{{ Phase 3
static token comment(token_stream &str, const token &t) {
  token nt = str.peek();
  if (nt.type == '*') {
    nt = str.take();
    while (str.has_more()) {
      nt = str.take();
      if (nt.type == '*' && str.peek().type == '/') {
        nt = str.take();
        break;
      }
    }
  } else if (nt.type == '/') {
    while (str.has_more() && nt.type != t_new_line) {
      nt = str.take();
    }
  } else {
    return t;
  }
  return token{.type = t_space, .begin = t.begin, .end = nt.end};
}

static hai::varray<token> phase_3(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  token_stream str{t};
  while (str.has_more()) {
    token t = str.take();
    if (t.type == '/') {
      res.push_back(comment(str, t));
    } else {
      res.push_back(t);
    }
  }
  return res;
}
// }}}

// {{{ Phase 4
static hai::varray<token> phase_4(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  for (auto c : t) {
    res.push_back_doubling(c);
  }
  return res;
}
// }}}

// {{{ Read-Eval-Print
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
// }}}

int main() {
  return yoyo::file_reader::open("example.cpp") //
      .fmap(preprocess_file)
      .map(success)
      .take(print_error);
}
