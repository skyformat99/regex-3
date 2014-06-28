#include "../reg_parse.h"
#include "../reg_state.h"
#include "../reg_malloc.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char const *argv[]){
  assert(argc>=2);
  const unsigned char* rule = (const unsigned char*)argv[1];

  printf("rule: %s\n", rule);

  struct reg_parse* p = parse_new();
  struct reg_state* s = state_new();

  struct reg_ast_node* root = parse_exec(p, rule, strlen((const char*)rule));
  parse_dump(root);

  state_gen(s, root);
  dump_frame(s);
  dump_edge(s);
  
  parse_free(p);
  state_free(s);

  reg_dump(); // print memory leakly
  return 0;
}