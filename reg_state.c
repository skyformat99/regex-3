#include "reg_malloc.h"
#include "reg_parse.h"
#include "reg_list.h"
#include <string.h>
#include <stdio.h>

#define DEF_EDGE 4

#define DEF_EDGES  128
#define DEF_NODES  DEF_EDGES*DEF_EDGE
#define DEF_FRAMES DEF_EDGES*2

#define frame_idx(p, idx) ((struct _range_frame*)list_idx(p->frame_list, idx))


struct reg_edge {
  struct reg_range range;
};

struct _range_frame {
  enum {
    e_begin,
    e_end
  }type;

  int value[2];
};

//  state node
struct reg_node {
  int node_idx;

  struct reg_list* edges;
};

struct reg_state{
  struct reg_list* state_list;
  struct reg_list* edges_list;

  struct reg_list* frame_list;
};

static void _gen_frame(struct reg_state* p, struct reg_ast_node* root);
static void _gen_edge(struct reg_state* p);
static void _gen_nfa(struct reg_state* p);
static void _gen_dfa(struct reg_state* p);


struct reg_state* state_new(){
  struct reg_state* ret = malloc(sizeof(struct reg_state));

  // init status nodes
  ret->state_list = list_new(sizeof(struct reg_node), DEF_NODES);
  struct reg_node* node = NULL;
  for(int i=0; (node = list_idx(ret->state_list, i)) != NULL; i++){
    node->edges = list_new(sizeof(int), DEF_EDGE);
    node->node_idx = i;
  }
  
  //init edges
  ret->edges_list = list_new(sizeof(struct reg_edge), DEF_EDGES);

  // init frame
  ret->frame_list = list_new(sizeof(struct _range_frame), DEF_FRAMES);
  return ret;
}

void state_free(struct reg_state* p){
  struct reg_node* node = NULL;
  // free state
  for(int i=0; (node = list_idx(p->state_list, i)) != NULL; i++){
    list_free(node->edges);
  }
  list_free(p->state_list);

  // free edges
  list_free(p->edges_list);

  // free frame
  list_free(p->frame_list);

  free(p);
}

void state_clear(struct reg_state* p){

}

static inline int _campar(const struct _range_frame* a, const struct _range_frame* b){
  return a->value[0] - b->value[0];
}


void state_gen(struct reg_state* p, struct reg_ast_node* ast){
  list_clear(p->frame_list);
  _gen_frame(p, ast);
  list_sort(p->frame_list, (campar)_campar);

  list_clear(p->edges_list);
  _gen_edge(p);

  list_clear(p->state_list);
  _gen_nfa(p);
  _gen_dfa(p);
}

static void _gen_frame(struct reg_state* p, struct reg_ast_node* root){
  if(!root) return;

  if(root->op == op_range){
    assert(root->childs[0] == NULL);
    assert(root->childs[1] == NULL);
    int begin = root->value.range.begin;
    int end   = root->value.range.end;

    struct _range_frame frame1 = {
      .type = e_begin,
      .value = {begin, end},
    };

    struct _range_frame frame2 = {
      .type = e_end,
      .value = {end, begin},
    };

    list_add(p->frame_list, &frame1);
    list_add(p->frame_list, &frame2);
    return;
  }

  _gen_frame(p, root->childs[0]);
  _gen_frame(p, root->childs[1]);
}

static inline void _insert_edge(struct reg_state* p, int begin, int end){
  struct reg_edge value = {
    .range.begin = begin,
    .range.end = end,
  };

  list_add(p->edges_list, &value);
}

static inline int _need_insert(struct reg_state* p, size_t idx){
  int value = frame_idx(p, idx)->value[0];
  int is_begin = 0, is_end = 0;

  struct _range_frame* v = NULL;
  for(;v = frame_idx(p, idx), v && value == v->value[0]; idx++){
    if(v->type == e_begin)
      is_begin = 1;
    if(v->type == e_end)
      is_end = 1;

    if(v->value[0] == v->value[1] || (is_begin && is_end))
      return 1;
  }
  return 0;
}

static inline int _read_frame(struct reg_state* p, size_t idx, 
  int* out_range_right, int* out_next_begin){
  
  int count = 0;
  assert(out_range_right);
  int value = frame_idx(p, idx)->value[0];
  
  int is_insert = 0;
  int is_begin = 0, is_end = 0;

  *out_next_begin = value;

  struct _range_frame* v = NULL;
  for(;  
      v = frame_idx(p, idx), v && value == v->value[0]; 
      idx++, count++){  

    // record end node 
    if(v->type == e_begin){
      is_begin = 1;
      int end = v->value[1];
      if(*out_range_right < end)
        *out_range_right = end;
    }

    if(v->type == e_end){
      is_end = 1;
    }

    // single range
    if((v->value[0] == v->value[1] || (is_begin && is_end)) && !is_insert){
      (*out_next_begin)++;
      _insert_edge(p, v->value[0], v->value[0]);
      is_insert = 1;
    }
  }

  return count;
}

static void _gen_edge(struct reg_state* p){
  int range_right = 0;
  int head = 0;
  size_t i = _read_frame(p, 0, &range_right, &head);

  struct _range_frame* frame = NULL;
  for(; (frame = list_idx(p->frame_list, i)); ){
    int tail = frame->value[0];
    if(_need_insert(p, i) || frame->type == e_begin){
      tail -= 1;
    }
    if(head <= tail && tail <= range_right){
      _insert_edge(p, head, tail);
    }

    int count = _read_frame(p, i, &range_right, &head);
    if(head == tail) head++;
    i += count;
  }
}

static void _gen_nfa(struct reg_state* p){

}

static void _gen_dfa(struct reg_state* p){

}



// for test
void dump_edge(struct reg_state* p){
  struct reg_edge* v = NULL;
  printf("------ dump_edge --------\n");
  for(size_t i=0; (v = list_idx(p->edges_list, i)); i++){
    printf("[%c - %c] ", v->range.begin, v->range.end);
  }
  printf("\n");
}

void dump_frame(struct reg_state* p){
 struct _range_frame* v = NULL;
 printf("-------- dump_frame ----------\n");
 for(size_t i=0; (v = list_idx(p->frame_list, i)); i++){
  printf("value: %c type: %d  ", v->value[0], v->type);
 }
 printf("\n");
}



