#pragma leco tool
#include <stdio.h>

import hai;
import missingno;
import traits;
import yoyo;

enum token_type : int {
  t_export = -10,
  t_module = -9,
  t_import = -8,
  t_pp_number = -7,
  t_raw_str = -6,
  t_str = -5,
  t_char = -4,
  t_identifier = -3,
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

  [[nodiscard]] bool matches(const char * txt) const {
    for (auto i = 0; txt[i] != 0; i++) {
      if (peek(i).type != txt[i])
        return false;
    }
    return true;
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
static token identifier(token_stream &str, const token &t);

// {{{ Utils
static bool is_digit(const token &t) {
  return t.type >= '0' && t.type <= '9';
}
static bool is_ident_start(const token &t) {
  char c = t.type;
  if (c >= 'A' && c <= 'Z')
    return true;
  if (c >= 'a' && c <= 'z')
    return true;
  if (c == '_')
    return true;

  return false;
}
static bool is_ident(const token &t) {
  return is_ident_start(t) || is_digit(t);
}
static bool is_non_nl_space(const token &t) {
  return t.type == ' ' || t.type == '\t'; // TODO: unicode space, etc
}
static bool is_type_modifier(const token &t) {
  return t.type == 'u' || t.type == 'U' || t.type == 'L';
}
static bool is_sign(const token &t) {
  return t.type == '+' || t.type == '-';
}
static token ud_suffix(token_stream &str, const token &t) {
  if (str.peek().type == '_' && is_ident_start(str.peek(1))) {
    str.skip(1);
    return identifier(str, str.take());
  }
  return t;
}
// }}}

static token comment(token_stream &str, const token &t) { // {{{
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
} // }}}
static token identifier(token_stream &str, const token &t) { // {{{
  token nt = t;
  while (is_ident(str.peek())) {
    nt = str.take();
  }
  return token{.type = t_identifier, .begin = t.begin, .end = nt.end};
} //}}}
static token non_nl_space(token_stream &str, const token &t) { // {{{
  token nt = t;
  while (is_non_nl_space(str.peek())) {
    nt = str.take();
  }
  return token{.type = t_space, .begin = t.begin, .end = nt.end};
} // }}}
static token char_literal(token_stream &str, const token &t) { // {{{
  token nt = str.take();
  while (str.has_more() && nt.type != '\'' && nt.type != t_new_line) {
    nt = str.take();
  }
  nt = ud_suffix(str, nt);
  return token{.type = t_char, .begin = t.begin, .end = nt.end};
} // }}}
static token str_literal(token_stream &str, const token &t) { // {{{
  token nt = str.take();
  while (str.has_more() && nt.type != '"' && nt.type != t_new_line) {
    nt = str.take();
  }
  nt = ud_suffix(str, nt);
  return token{.type = t_str, .begin = t.begin, .end = nt.end};
} // }}}
static token raw_str_literal(token_stream &str, const token &t) { // {{{
  token nt = str.take();
  if (nt.type != '(')
    throw R"TBD(TBD)TBD";

  while (str.has_more() && !(nt.type == ')' && str.peek().type == '"')) {
    nt = str.take();
  }

  nt = str.take(); // takes "
  nt = ud_suffix(str, nt);
  return token{.type = t_raw_str, .begin = t.begin, .end = nt.end};
} // }}}
static token pp_number(token_stream &str, const token &t) { // {{{
  token nt = t;
  while (str.has_more()) {
    auto tt = str.peek();
    if (is_ident(tt)) {
      nt = str.take();
      continue;
    }
    auto ttt = str.peek(1);
    if (tt.type == '\'' && (is_digit(ttt) || is_ident_start(ttt))) {
      str.skip(1);
      nt = str.take();
      continue;
    }
    if ((tt.type == 'e' || tt.type == 'E' || tt.type == 'p' ||
         tt.type == 'P') &&
        is_sign(ttt)) {
      str.skip(1);
      nt = str.take();
      continue;
    }
    if (tt.type == '.') {
      nt = str.take();
      continue;
    }

    break;
  }
  return token{.type = t_pp_number, .begin = t.begin, .end = nt.end};
} // }}}

static hai::varray<token> phase_3(const hai::varray<token> &t) {
  hai::varray<token> res{t.size()};
  token_stream str{t};
  while (str.has_more()) {
    if (str.matches("export")) {
      token t = str.peek();
      t.type = t_export;
      t.end = t.begin + 5;
      str.skip(6);
      res.push_back(t);
    }
    if (str.matches("import")) {
      token t = str.peek();
      t.type = t_import;
      t.end = t.begin + 5;
      str.skip(6);
      res.push_back(t);
    }
    if (str.matches("module")) {
      token t = str.peek();
      t.type = t_module;
      t.end = t.begin + 5;
      str.skip(6);
      res.push_back(t);
    }

    token t = str.take();
    if (t.type == '/') {
      res.push_back(comment(str, t));
      continue;
    }

    if (is_digit(t)) {
      res.push_back(pp_number(str, t));
      continue;
    }
    if (t.type == '.' && is_digit(str.peek())) {
      res.push_back(pp_number(str, t));
      continue;
    }

    if (t.type == '\'') {
      res.push_back(char_literal(str, t));
      continue;
    }
    if (t.type == 'u' && str.peek().type == '8' && str.peek(1).type == '\'') {
      str.skip(2);
      res.push_back(char_literal(str, t));
      continue;
    }
    if (is_type_modifier(t) && str.peek().type == '\'') {
      str.skip(1);
      res.push_back(char_literal(str, t));
      continue;
    }

    if (t.type == '"') {
      res.push_back(str_literal(str, t));
      continue;
    }
    if (t.type == 'u' && str.peek().type == '8' && str.peek(1).type == '"') {
      str.skip(2);
      res.push_back(str_literal(str, t));
      continue;
    }
    if (is_type_modifier(t) && str.peek().type == '"') {
      str.skip(1);
      res.push_back(str_literal(str, t));
      continue;
    }

    if (t.type == 'R' && str.peek().type == '"') {
      str.skip(1);
      res.push_back(raw_str_literal(str, t));
      continue;
    }
    if (t.type == 'u' && str.peek().type == '8' && str.peek(1).type == 'R' &&
        str.peek(2).type == '"') {
      str.skip(3);
      res.push_back(raw_str_literal(str, t));
      continue;
    }
    if (is_type_modifier(t) && str.peek().type == 'R' &&
        str.peek(1).type == '"') {
      str.skip(2);
      res.push_back(raw_str_literal(str, t));
      continue;
    }

    if (is_non_nl_space(t)) {
      res.push_back(non_nl_space(str, t));
      continue;
    }
    if (is_ident_start(t)) {
      res.push_back(identifier(str, t));
      continue;
    }

    res.push_back(t);
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
static mno::req<hai::cstr> slurp(const char *name) {
  return yoyo::file_reader::open(name) //
      .fmap([](auto &f) {
        return f.size()
            .map([](auto sz) { return hai::cstr{sz}; })
            .fmap([&](auto &&buf) {
              return f.read(buf.data(), buf.size()).map([&] {
                return traits::move(buf);
              });
            });
      });
}
static int preprocess_file(const hai::cstr &buf) {
  auto tokens = phase_4(phase_3(phase_2(phase_1(buf))));

  for (auto t : tokens) {
    // printf("%3d %5d %5d\n", t.type, t.begin, t.end);
    printf("[%.*s]", (t.end - t.begin + 1), buf.begin() + t.begin);
  }
  return 0;
}

static int print_error(const char *err) {
  fprintf(stderr, "%s", err);
  return 1;
}
// }}}

int main() {
  return slurp("tests/example.cpp") //
      .map(preprocess_file)
      .take(print_error);
}
