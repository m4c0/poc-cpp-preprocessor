#pragma leco tool
import hai;
import jojo;
import jute;
import print;

enum token_type : int {
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
  unsigned line;
  unsigned column;
};

// {{{ Token Stream
class token_stream {
  const hai::chain<token> & m_tokens;
  unsigned offset{};

  token eof() const {
    const auto &last = m_tokens.seek(m_tokens.size() - 1);
    return token{.type = t_eof, .begin = last.end + 1, .end = last.end + 1};
  }

public:
  explicit token_stream(const hai::chain<token> & t) : m_tokens(t) {}

  bool has_more() { return offset < m_tokens.size(); }

  void skip(unsigned n) {
    offset = (offset + n >= m_tokens.size()) ? m_tokens.size() : offset + n;
  }
  [[nodiscard]] token take() {
    if (offset >= m_tokens.size()) return eof();
    return m_tokens.seek(offset++);
  }
  [[nodiscard]] token peek(unsigned d = 0) const {
    if (offset + d >= m_tokens.size()) return eof();
    return m_tokens.seek(offset + d);
  }

  [[nodiscard]] bool matches(const char *txt) const {
    for (auto i = 0; txt[i] != 0; i++) {
      if (peek(i).type != txt[i])
        return false;
    }
    return true;
  }
};
// }}}

// {{{ Phase 1
static auto phase_1(const hai::cstr &file) {
  hai::chain<token> res { file.size() };
  unsigned line = 1;
  unsigned column = 1;
  for (auto i = 0U; i < file.size(); i++) {
    switch (auto c = file.data()[i]) {
    case 0xd:
      if (file.data()[i + 1] == 0xa) i++;
      res.push_back(token {
          .type = t_new_line,
          .begin = i, .end = i,
          .line = line, .column = column });
      line++;
      column = 1;
      break;
    default:
      // TODO: form translation charset elements
      // https://en.cppreference.com/w/cpp/language/charset#Translation_character_set
      res.push_back(token{
          .type = static_cast<token_type>(c),
          .begin = i, .end = i,
          .line = line, .column = column });
      if (c == 0xa) {
        line++;
        column = 1;
      } else {
        column++;
      }
    }
  }
  return res;
}
// }}}

// {{{ Phase 2
static auto phase_2(const hai::chain<token> & t) {
  hai::chain<token> res { t.size() };
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

  if (res.size() > 0 && res.seek(res.size() - 1).type != t_new_line) {
    const auto &last = res.seek(res.size() - 1);
    res.push_back(token{
        .type = t_new_line,
        .begin = last.end + 1,
        .end = last.end + 1,
        .line = last.line,
        .column = last.column,
    });
  }
  return res;
}
// }}}

// {{{ Phase 3
static token identifier(token_stream &str, const token &t);

// {{{ Utils
static bool is_digit(const token &t) { return t.type >= '0' && t.type <= '9'; }
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
static bool is_sign(const token &t) { return t.type == '+' || t.type == '-'; }
static token ud_suffix(token_stream &str, const token &t) {
  if (str.peek().type == '_' && is_ident_start(str.peek(1))) {
    str.skip(1);
    return identifier(str, str.take());
  }
  return t;
}
static token merge(token_type type, token t, token nt) {
  return token {
    .type = type,
    .begin = t.begin, .end = nt.end,
    .line = t.line, .column = t.column };
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
  return merge(t_space, t, nt);
} // }}}
static token identifier(token_stream &str, const token &t) { // {{{
  token nt = t;
  while (is_ident(str.peek())) {
    nt = str.take();
  }
  return merge(t_identifier, t, nt);
} //}}}
static token non_nl_space(token_stream &str, const token &t) { // {{{
  token nt = t;
  while (is_non_nl_space(str.peek())) {
    nt = str.take();
  }
  return merge(t_space, t, nt);
} // }}}
static token char_literal(token_stream &str, const token &t) { // {{{
  token nt = str.take();
  while (str.has_more() && nt.type != '\'' && nt.type != t_new_line) {
    nt = str.take();
  }
  nt = ud_suffix(str, nt);
  return merge(t_char, t, nt);
} // }}}
static token str_literal(token_stream &str, const token &t) { // {{{
  token nt = str.take();
  while (str.has_more() && nt.type != '"' && nt.type != t_new_line) {
    nt = str.take();
  }
  nt = ud_suffix(str, nt);
  return merge(t_str, t, nt);
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
  return merge(t_raw_str, t, nt);
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
  return merge(t_pp_number, t, nt);
} // }}}

static auto phase_3(const hai::chain<token> &t) {
  hai::chain<token> res{ t.size() };
  token_stream str{t};
  while (str.has_more()) {
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
static void consume_space(token_stream &str) {
  while (str.peek().type == t_space) {
    str.skip(1);
  }
}

static auto phase_4(const hai::chain<token> & t) {
  hai::chain<token> res { t.size() };
  token_stream str{t};
  while (str.has_more()) {
    consume_space(str);

    auto t = str.take();
    if (t.type == '#') {
      consume_space(str);

      auto nt = t;
      while (str.has_more() && nt.type != t_new_line) {
        nt = str.take();
      }
      t.end = nt.end;
      res.push_back(t);
      continue;
    }
    if (t.type == t_import) {
      consume_space(str);
      continue;
    } else if (t.type == t_module) {
      consume_space(str);
      continue;
    }
    res.push_back(t);
    while (str.has_more() && t.type != t_new_line) {
      t = str.take();
      res.push_back(t);
    }
  }
  return res;
}
// }}}

int main() try {
  jute::view fn = "tests/example.cpp";
  auto buf = jojo::read_cstr(fn);
  auto tokens = phase_4(phase_3(phase_2(phase_1(buf))));

  for (auto t : tokens) {
    put(fn, ':', t.line, ':', t.column, ": ");
    putfn("[%.*s]", (t.end - t.begin + 1), buf.begin() + t.begin);
  }
} catch (...) {
  return 1;
}
