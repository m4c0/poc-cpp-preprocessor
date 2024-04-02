#pragma leco tool
#include <stdio.h>

import missingno;
import yoyo;

struct token {
  char c;
};

static token phase_1(token t) { return t; }

static token phase_2(token t) { return t; }

static token phase_3(token t) { return t; }

static token phase_4(token t) { return t; }

static token to_token(char c) { return {c}; }
static void print_token(token t) { putc(t.c, stdout); }

static bool identity(bool b) { return b; }

static mno::req<void> preprocess_file(yoyo::reader &f) {
  return f.read_s8()
      .map(to_token)
      .map(phase_1)
      .map(phase_2)
      .map(phase_3)
      .map(phase_4)
      .map(print_token) //
      .fmap([&] { return preprocess_file(f); })
      .if_failed([&](auto msg) {
        return f.eof().assert(identity, msg).map([](auto) {});
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
