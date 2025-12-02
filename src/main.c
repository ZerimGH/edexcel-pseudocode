#include "def.h"
#include "parser.h"
#include "tokeniser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Read a file into a string
char *read_file_str(char *file_dir) {
  if(!file_dir) {
    PERROR("NULL file_dir passed.\n");
    return NULL;
  }

  FILE *file = fopen(file_dir, "rb");
  if(!file) {
    PERROR("Could not open file %s.\n", file_dir);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  size_t file_len = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *out = malloc(sizeof(char) * (file_len + 1));
  if(!out) {
    PERROR("malloc() failed.\n");
    fclose(file);
    return NULL;
  }

  if(fread(out, sizeof(char), file_len, file) != file_len) {
    PERROR("fread() failed.\n");
    free(out);
    fclose(file);
    return NULL;
  }

  out[file_len] = '\0';
  return out;
}

// Strip comments from a string (# until \n)
void strip_comments(char *src) {
  if(!src) return;
  char *read = src;
  char *write = src;

  int in_comment = 0;

  while(*read != '\0') {
    if(*read == '#') in_comment = 1;
    if(*read == '\n') in_comment = 0; 
    if(!in_comment) *(write++) = (*read);
    read++;
  }
  *write = '\0';
}

void print_help(char *bin_path) {
  printf("Usage: %s [options] file\n", bin_path);
  printf("Options:\n");
  printf("--help                  Show this help message\n");
  printf("-t, --tokeniser_debug   Print a debug view of the tokeniser after tokenisation\n");
  printf("-T, --tokenise_only     Tokenise only; do not parse or execute\n");
  printf("-p, --parser_debug      Print a debug view of the parser after parsing\n");
  printf("-P, --parse_only        Tokenise and parse only; do not execute\n");
  printf("Note: Combining multiple short flags like -Tt is not supported.\n");
}

int main(int argc, char **argv) {
  // Display help if run without args
  if(argc < 2) {
    print_help(argv[0]);
    return 0;
  }

  // Parse arguments
  char *file_path = NULL;
  int help = 0;
  int tok_debug = 0;
  int tok_only = 0;
  int parse_debug = 0;
  int parse_only= 0;
  for(int i = 1; i < argc - 1; i++) {
    if(strcmp(argv[i], "--help") == 0) help = 1;
    if(strcmp(argv[i], "--tokeniser_debug") == 0) tok_debug = 1;
    if(strcmp(argv[i], "-t") == 0) tok_debug = 1;
    if(strcmp(argv[i], "--tokenise_only") == 0) tok_only = 1;
    if(strcmp(argv[i], "-T") == 0) tok_only = 1;
    if(strcmp(argv[i], "--parser_debug") == 0) parse_debug = 1;
    if(strcmp(argv[i], "-p") == 0) parse_debug = 1;
    if(strcmp(argv[i], "--parse_only") == 0) parse_only = 1;
    if(strcmp(argv[i], "-P") == 0) parse_only = 1;
  }

  if(help) {
    // Display help message and exit early
    print_help(argv[0]);
    return 0;
  }

  // File path should always be the last argument
  file_path = argv[argc - 1];

  // Read input file into a string
  char *file_contents = read_file_str(file_path);
  if(!file_contents) {
    PERROR("Failed to read input file.\n");
    return 1;
  }
  // Remove comments
  strip_comments(file_contents);

  // Tokenise the input
  Tokeniser *tokeniser = tokenise(file_contents);
  free(file_contents);
  if(!tokeniser) {
    PERROR("Failed to tokenise file.\n");
    return 1;
  }

  if(tok_debug) tokeniser_dump(tokeniser);
  if(tok_only) {
    // Exit early
    tokeniser_destroy(&tokeniser);
    return 0;
  }

  // Parse the tokens
  Parser *parser = parse(tokeniser);
  tokeniser_destroy(&tokeniser);
  if(!parser) {
    PERROR("Failed to parse tokens.\n");
    return 1;
  }

  if(parse_debug) parser_dump(parser);
  if(parse_only) {
    // Exit early
    parser_destroy(&parser);
    return 0;
  }

  parser_destroy(&parser);

  return 0;
}
