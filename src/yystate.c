#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set.h>
#include <hash.h>
#include <compiler.h>
#include "parser.h"
#include "llout.h"			/* for _EOI_ definition */

/* for statistics only: */
static int Nitems         = 0;  /* number of LALR(1) items */
static int Npairs         = 0;  /* # of pairs in output tables */
static int Ntab_entries   = 0;  /* number of transitions in tables */
static int Shift_reduce   = 0;  /* number of shift/reduce conflicts */
static int Reduce_reduce  = 0;  /* number of reduce/reduce conflicts */ 


#define MAXSTATE 512  /* max # of LALR(1) states */
#define MAXOBUF 256   /* buffer size for various output routines */

typedef struct _item_ {   /* LALR(1) item */
  int prod_num;           /* production number */
  PRODUCTION *prod;       /* the production itself */
  SYMBOL *right_of_dot;   /* symbol to the right of the dot */
  unsigned int dot_posn;  /* offset of dot form start of production */
  SET *lookaheads;        /* set of lookahead symbol for this item */
} ITEM;

#define RIGHT_OF_DOT(p) ((p)->right_of_dot ? (p)->right_of_dot->val : 0)

#define MAXKERNEL   32      /* maximum number of kernel items in a state */
#define MAXCLOSE    128     /* maximum number of closure items in a state (less the epsilon productions)*/
#define MAXEPSILON  8       /* maximum number of epsilon productions that can be in a closure set for any given state */

typedef struct _state {   /* LALR(1) state */
  ITEM *kernel_items[MAXKERNEL];    /* set of kernel items */
  ITEM *epsilon_items[MAXEPSILON];  /* set of epsilon items */

  unsigned int nkitems;  /* items in kernel_items[] */
  unsigned int neitems;  /* items in epsilon_items[] */
  unsigned int closed;   /* state has had closure performed */

  unsigned int num;      /* state number (0 is start state) */
} STATE;


typedef struct act_or_goto {
  int sym;                    /* given this input symbol */
  int do_this;                /* do this. >0 == shift <0 == reduce */
  struct act_or_goto *next;   /* pointer to next ACT in the linked list */ 
} ACT;


typedef ACT GOTO;   /* GOTO is an alias for ACT */

static ACT *Actions[MAXSTATE]; /* array of pointers to the head of the action chains. 
                                * indexed by state number. i'm counting on initialization 
                                * to NULL here.
                                */

static GOTO *Gotos[MAXSTATE];  /* array of pointers to the head of the goto chains */



#define CHUNK 128                 /* New gets this many structures at once */
static HASH_TAB *States = NULL;   /* LALR(1) states */
static int Nstates = 0;           /* number of states */

#define MAX_UNFINISHED 128

typedef struct tnode {
  STATE *state;
  struct tnode *left, *right;
} TNODE;


static TNODE Heap[MAX_UNFINISHED];  /* source of all TNODEs */
static TNODE *Next_allocate = Heap; /* ptr to next node to allocate */

static TNODE *Available = NULL;     /* free list of available nodes 
                                     * linked list of TNODEs. p->left is used
                                     * as the link
                                     */

static TNODE *Unfinished = NULL; /* tree of unfinished states */

static ITEM **State_items;      /* used to pass info to state_cmp */
static int State_nitems;
static int Sort_by_number;

#define NEW 0       /* possible return values from newstate() */
#define UNCLOSED 1  
#define CLOSED 2

ITEM *Recycled_items = NULL;

#define MAX_TOK_PER_LINE 15
static int Tokens_printed; /* controls number of lookaheads printed on a single line of yyout.doc */


static void *new(void) 
{
  /* return an area of memory that can be used as either an ACT or GOTO.
   * these objects cannot be freed.
   */
  
  static ACT *eheap; /* assuming default initialization to NULL here */
  static ACT *heap;  /* ditto */

  if (heap >= eheap) { 
    if (!(heap = (ACT *) malloc(sizeof(ACT) * CHUNK))) {
      error(FATAL, "no memory for action or goto\n");
    }
    eheap = heap + CHUNK;
  }
  
  ++Ntab_entries;
  return heap++;
}



static ACT *p_action(int state, int input_sym)
{
  /* return a pointer to the existing ACT structure representing the indicated
   * state and input symbol (or NULL if no such symbol exists).
   */
  
  ACT *p;

  if (state >= MAXSTATE) {
    error(FATAL, "bad state argument to p_action (%d)\n", state);
  }

  for (p = Actions[state]; p; p = p->next) {
    if (p->sym == input_sym) {
      return p;
    }
  }

  return NULL;
}

static void add_action(int state, int input_sym, int do_this)
{
  /* add an element to the action part of the parse table. the cell is
   * indexed by the state number and input symbol, and holds do_this.
   */

  ACT *p;
  if (state > MAXSTATE) {
    error(FATAL, "bad state argument to add_action (%d)\n", state);
  }

  if (p = p_action(state, input_sym)) {
    error(FATAL, "tried tp add duplicate action in state %d: \n"
                 "  (1) shift/reduce %d on %s <-existing\n"
                 "  (2) shift/reduce %d on %s <-new\n",
                 state, p->do_this, Terms[p->sym]->name, do_this, Terms[input_sym]->name);
  }

  if (Verbose > 1) {
    printf("adding shift or reduce action from state %d: %d on %s\n",
            state, do_this, Terms[input_sym]->name);
  }

  p = (ACT *) new();
  p->sym = input_sym;
  p->do_this = do_this;
  p->next = Actions[state];
  Actions[state] = p;
}


static GOTO *p_goto(int state, int nonterminal)
{
  /* return a pointer to the existing GOTO structure representing the
   * indicated state and nonterminal (or NULL if no such symbol exists). the
   * value used for the nonterminal is the one in the symbol table; it is
   * adjusted down (so that the smallest nonterminal has the value 0)
   * before doing the table look up, however.
   */
  
  GOTO *p;
  int unadjusted = nonterminal;   /* original value of nonterminal */

  if (state > MAXSTATE) {
    error(FATAL, "bad state argument to p_goto (%d)\n", state);
  }

  nonterminal = ADJ_VAL(nonterminal);
  if (nonterminal >= NUMNONTERMS) {
    error(FATAL, "bad nonterminal argument to p_goto (%d)\n", unadjusted);
  }

  for (p = Gotos[state]; p; p = p->next) {
    if (p->sym == nonterminal) {
      return p;
    }
  }

  return NULL;
}

static void add_goto(int state, int nonterminal, int go_here)
{
  /* add an element to the goto part of the parse table, the cell is indexed
   * by current state number and nonterminal value, and holds go_here. note
   * that the input nonterminal value is the one that appears in the symbol
   * table. it is adjusted downwards (so that the smallest nonterminal will
   * have the value 0) before being inserted into the table, however.
   */
  
  GOTO *p;
  int unadjusted = nonterminal;   /* original value of nonterminal */
  nonterminal = ADJ_VAL(nonterminal);

  if (state > MAXSTATE) {
    error(FATAL, "bad state argument to add_goto (%d)\n", state);
  }

  if (nonterminal >= NUMNONTERMS) {
    error(FATAL, "bad nonterminal argument to add_goto (%d)\n", unadjusted);
  }

  if (p = p_goto(state, unadjusted)) {
    error(FATAL, "tried to add duplicate goto on nonterminal %s\n"
                 "  (1) goto %3d from %3d <-existing\n"
                 "  (2) goto %3d from %3d <-new\n",
                 Terms[unadjusted]->name,
                 p->sym, p->do_this, go_here, state);
  }

  if (Verbose > 1) {
    printf("adding goto from state %d to %d on %s\n", 
              state, go_here, Terms[unadjusted]->name);
  }

  p = (GOTO *) new();
  p->sym = nonterminal;
  p->do_this = go_here;
  p->next = Gotos[state];
  Gotos[state] = p;
}

static void sprint_tok(char **bp, char *format, int arg)
{
  /* format: not used here, but supplied by pset() */

  /* print one nonterminal symbol to a buffer maintained by the
   * calling routine and update the calling routine's pointer
   */

  if (arg == -1) {
    *bp += sprintf(*bp, "null ");
  } else if (arg == -2) {
    *bp += sprintf(*bp, "empty ");
  } else if (arg == _EOI_) {
    *bp += sprintf(*bp, "$ ");
  } else if (arg == EPSILON) {
    *bp += sprintf(*bp, "");
  } else {
    *bp += sprintf(*bp, "%s ", Terms[arg]->name);
  }

  if (++Tokens_printed >= MAX_TOK_PER_LINE) {
    *bp += sprintf(*bp, "\n\t\t");
    Tokens_printed = 0;
  }
}

static char *stritem(ITEM *item, int lookaheads)
{
  /* return a pointer to a string that holds a representation of an item.
   * The lookaheads are printed too if "lookaheads" is true or Verbose
   * is > 1 (-V was specified on the command line).
   */
  static char buf[MAXOBUF * 4];
  char *bp;
  int i;

  bp = buf;
  bp += sprintf(bp, "%s->", item->prod->lhs->name);

  if (item->prod->rhs_len <= 0) {
    bp += sprintf(bp, "<epsilon>. ");
  } else {
    for (i = 0; i < item->prod->rhs_len; i++) {
      if (i == item->dot_posn) {
        *bp++ = '.';
      }

      bp += sprintf(bp, " %s", item->prod->rhs[i]->name);
    }

    if (i == item->dot_posn) {
      *bp++ = '.';
    }
  }

  if (lookaheads || Verbose > 1) {
    bp += sprintf(bp, " (production %d, precedence %d)\n\t\t[", item->prod_num, item->prod->prec);
    Tokens_printed = 0;
    pset(item->lookaheads, (pset_t)sprint_tok, &bp);
    *bp++ = ']';
  }

  if (bp >= &buf[sizeof(buf)]) {
    error(FATAL, "stritem buffer overflow\n");
  }

  *bp = '\0';
  return buf;
}

static void pstate(STATE *state)
{
  /* print one row of the parse table in human-readable form yyout.doc
   * (stderr if -V is specified).
   */
  
  int i;
  ITEM **item;
  ACT *p;

  document("state %d:\n", state->num);
  /* print the kernel and epsilon items for the current state */

  for (i = state->nkitems, item = state->kernel_items; --i >= 0; ++item) {
    document("  %s\n", stritem(*item, (*item)->right_of_dot == 0));
  }

  for (i = state->neitems, item = state->epsilon_items; --i >= 0; ++item) {
    document("  %s\n", stritem(*item, 1));
  }

  document("\n");

  /* print out the next-state transitions, first the actions, then the gotos */

  for (i = 0; i < (MINTERM + USED_TERMS); ++i) {
    if (p = p_action(state->num, i)) {
      if (p->do_this == 0) {
        if (p->sym == _EOI_) {
          document("  accept on end of input\n");
        } else if (p->sym == WHITESPACE) {
          document("  accept on whitespace\n");
        } else {
          error(FATAL, "state: %d illegal accept", state->num);
        }
      } else if (p->do_this < 0) {
        document("  reduce by %d on %s\n", -(p->do_this), Terms[p->sym]->name);
      } else {
        document("  shift to %d on %s\n", p->do_this, Terms[p->sym]->name);
      }
    }
  }

  for (i = MINNONTERM; i < MINNONTERM + USED_NONTERMS; i++) {
    if (p = p_goto(state->num, i)) {
      document("  goto %d on %s\n", p->do_this, Terms[i]->name);
    }
  }
  document("\n");
}

static void pstate_stdout(STATE *state)
{
  document_to(stdout);
  pstate(state);
  document_to(NULL);
}

static void pclosure(STATE *kernel, ITEM **closure_items, int nitems)
{
  printf("\n%d items in closure of ", nitems);
  pstate_stdout(kernel);

  if (nitems > 0) {
    printf("  -----closure items:----\n");
    while (--nitems >=0) {
      printf("  %s\n", stritem(*closure_items++, 0));
    }
  }
}

int newstate(ITEM **items, int nitems, STATE **statep)
{
  STATE *state;
  STATE *existing;
  
  if (nitems > MAXKERNEL) {
    error(FATAL, "kernel of new state %d too large\n", Nstates);
  }

  State_items = items;    /* set up parameters for state_cmp */
  State_nitems = nitems;  /* and state_hash */

  if (existing = (STATE *) findsym(States, NULL)) {
    /* state exists; by not setting "state" to NULL, we'll recycle
	   * the newly allocated state on the next call
     */

    *statep = existing;
    if (Verbose > 1) {
      printf("using existing state (%sclosed): ", existing->closed ? "" : "un");
      pstate_stdout(existing);
    }

    return existing->closed ? CLOSED : UNCLOSED;
  } else {
    if (Nstates >= MAXSTATE) {
      error(FATAL, "too many LALR(1) states\n");
    }

    if (!(state = (STATE *) newsym(sizeof(STATE)))) {
      error(FATAL, "no memeory for states\n");
    }

    memcpy(state->kernel_items, items, nitems * sizeof(ITEM*));
    state->nkitems = nitems;
    state->neitems = 0;
    state->closed = 0;
    state->num = Nstates++;
    *statep = state;
    addsym(States, state);

    if (Verbose > 1) {
      printf("forming new state:");
      pstate_stdout(state);
    }

    return NEW;
  }
}


static void add_unfinished(STATE *state)
{
  TNODE **parent, *root;
  int cmp;

  parent = &Unfinished;
  root = Unfinished;
  while (root) { /* look for the node in the tree */
    if ((cmp = state->num - root->state->num) == 0) {
      break;
    } else {
      parent = (cmp < 0) ? &root->left : &root->right;
      root = (cmp < 0) ? root->left : root->right;
    }
  }

  if (!root) { /* node isn't in tree. allocate a new node and put it into the tree */
    if (Available) {
      *parent = Available;    /* use node from available list if possible */
      Available = (TNODE *)Available->left;
    } else {  /* otherwise get the node from the Heap */
      if (Next_allocate >= &Heap[MAX_UNFINISHED]) {
        error(FATAL, "no memory for unfinished state\n");
      }
      *parent = Next_allocate++;
    }
    (*parent)->state = state;           /* initialize the node */
    (*parent)->left = (*parent)->right = NULL;
  }
}


static STATE *get_unfinished()
{
  /* returns a pointer to the next unfinished state and deletes that
   * state from the unfinished tree. returns NULL if the tree is empty.
   */
  TNODE *root;
  TNODE **parent;

  if (!Unfinished) {
    return NULL;
  }

  parent = &Unfinished;
  root = Unfinished;    /* find leftmost node */

  while(root->left) {
    parent = &root->left;
    root = root->left;
  }

  *parent = root->right;
  root->left = Available;
  Available = root;
  
  return root->state;
}

static char *strprod(PRODUCTION *prod)
{
  /* return a pointer to a string that holds a representation
   * of a production
   */
  
  static char buf[MAXOBUF];
  char *bp;
  int i;
  bp = buf;
  bp += sprintf(bp, "%s ->", prod->lhs->name);

  if (prod->rhs_len <= 0) {
    bp += sprintf(bp, " <epsilon>");
  } else {
    for (i = 0; i < prod->rhs_len; i++) {
      bp += sprintf(bp, " %s", prod->rhs[i]->name);
    }
  }

  if (bp - buf >= MAXOBUF) {
    error(FATAL, "strprod buffer overflow\n");
  }
  *bp = '\0';

  return buf;
}

static ITEM *newitem(PRODUCTION *production)
{
  ITEM *item;

  if (Recycled_items) {
    item = Recycled_items;
    Recycled_items = (ITEM *) Recycled_items->prod;
    CLEAR(item->lookaheads);
  } else {
    if (!(item = (ITEM *) malloc(sizeof(ITEM)))) {
      error(FATAL, "no memory for LALR(1) items\n");
    }
    item->lookaheads = newset();
  }
  
  if (Verbose > 1) {
    printf("making new item for %s\n", strprod(production));
  }

  ++Nitems;
  item->prod = production;
  item->prod_num = production->num;
  item->dot_posn = 0;
  item->right_of_dot = production->rhs[0];
  return item;
}

static void freeitem(ITEM *item) {
  --Nitems;
  item->prod = (PRODUCTION *) Recycled_items;
  Recycled_items = item;
}

static void free_recycled_items()
{
  /* empty the recycling list, freeing all memory used by items there */

  ITEM *p;

  while (p = Recycled_items) {
    Recycled_items = (ITEM *) Recycled_items->prod;
    free(p);
  }
}

static void movedot(ITEM *item)
{
  /* moves the dot one position to the right and updates the right_of_dot
   * symbol
   */
  
  if (item->right_of_dot == NULL) {
    error(FATAL, "illegal movedot() call on epsilon production\n");
  }

  item->right_of_dot = (item->prod->rhs)[++item->dot_posn];
}


static int item_cmp(const void *item1p, const void *item2p)
{
  /* return the relative weight of two items, 0 if they're equivalent */

  int rval;
  ITEM *item1 = *(ITEM **) item1p;
  ITEM *item2 = *(ITEM **) item2p;

  if (!(rval = RIGHT_OF_DOT(item1) - RIGHT_OF_DOT(item2))) {
    if (!(rval = item1->prod_num - item2->prod_num)) {
      return item1->dot_posn - item2->dot_posn;
    }
  }

  return rval;
}

static int merge_lookaheads(ITEM **dst_items, ITEM **src_items, int nitems)
{
  /* this routine is called if newstate has determined that a state having the
   * specified items already exists. if this is the case, the item list in the
   * STATE and the current item list will be identical in all respects except
   * lookaheads. This routine merges the lookaheads of the input items
   * (src_items) to the items already in the state (dst_items). 0 is returned
   * if nothing was done (all lookaheads in the new state are already in the
   * existing state), 1 otherwise. it's an internal error if the items don't
   * match.
   */
  
  int did_something = 0;
  
  while (--nitems >= 0) {
    if ((*dst_items)->prod != (*src_items)->prod || (*dst_items)->dot_posn != (*src_items)->dot_posn) {
      error(FATAL, "merge_lookahead item mismatch");
    }

    if (!subset((*dst_items)->lookaheads, (*src_items)->lookaheads)) {
      ++did_something;
      UNION((*dst_items)->lookaheads, (*src_items)->lookaheads);
    }
    ++dst_items;
    ++src_items;
  }

  return did_something;
}


static int move_eps(STATE *cur_state, ITEM **closure_items, int nclose)
{
  /* move the epsilon items from the closure_items set to the kernel of the
   * current state. if epsilon items already exist in the current state,
   * just merge the lookaheads. Note that, because the closure items were
   * sorted to partition them, the epsilon productions in the closure_items
   * set will be in the same order as those already in the kernel. return
   * the number of items that were moved.
   */
  
  ITEM **eps_items, **p;
  int nitems, moved;

  eps_items = cur_state->epsilon_items;
  nitems = cur_state->neitems;
  moved = 0;

  for (p = closure_items; --nclose >= 0 && (*p)->prod->rhs_len ==0; ) {
    if (++moved > MAXEPSILON) {
      error(FATAL, "too many epsilon productions in state %d\n", cur_state->num);
    }
    if (nitems) {
      UNION((*eps_items++)->lookaheads, (*p++)->lookaheads);
    } else {
      *eps_items++ = *p++;
    }
  }

  if (moved) {
    cur_state->neitems = moved;
  }
  return moved;
}


static int kclosure(STATE *kernel, ITEM **closure_items, int maxitems, int nclose)
{
  /* kernel: kernel state to close 
   * closure_items: array into which closure items are put
   * maxitems: size of the closure_items[] array
   * nclose: # of items already in set
   * /
  
  /* add to the closure set those items from the kernel that will shift to new states 
   * (ie. the items with dots somewhere other than the far right)
   */
  
  int nitems;
  ITEM *item, **itemp, *citem;

  closure_items += nclose;
  maxitems -= nclose;

  itemp = kernel->kernel_items;
  nitems = kernel->nkitems;

  while (--nitems >= 0) {
    item = *itemp++;
    if (item->right_of_dot) {
      citem = newitem(item->prod);
      citem->prod = item->prod;
      citem->dot_posn = item->dot_posn;
      citem->right_of_dot = item->right_of_dot;
      citem->lookaheads = dupset(item->lookaheads);
      if (--maxitems < 0) {
        error(FATAL, "too many closure items in state %d\n", kernel->num);
      }
      *closure_items++ = citem;
      ++nclose;
    }
  }

  return nclose;
}

static ITEM *in_closure_items(PRODUCTION *production, ITEM **closure_items, int nitems)
{
  /* if the indicated production is in the closure_items already, return a
   * pointer to the existing item, otherwise return NULL.
   */
  
  for (; --nitems >= 0; ++closure_items) {
    if ((*closure_items)->prod == production) {
      return *closure_items;
    }
  }

  return NULL;
}

static int add_lookahead(SET *dst, SET *src)
{
  /* merge the lookaheads in the src and dst sets. if the original src
   * set was empty, or if it was already a subset of the destination set,
   * return 0, otherwise return 1.
   */
  
  if (!IS_EMPTY(src) && !subset(dst, src)) {
    UNION(dst, src);
    return 1;
  }

  return 0;
}

static int do_close(ITEM *item, ITEM *closure_items[], int *nitems, int *maxitems)
{
  /* closure_items: (output) array of items added by closure process 
   * nitems: (input) # of items currently in closure_items[] 
   *         (output) # of items in closure_items after processing
   * maxitems: (input) max # of items that can be added
   *           (output) input adjusted for newly added items
   */

  /* workhorse function used by closure(). performs LR(1) closure on the
   * input item ([A->b.Cd, e] add [C->x, FIRST(de)]). the new items are added
   * to the closure_items[] array and *nitems and *maxitems are modified to
   * reflect the number of items in the closure set. return 1 if do_close()
   * did anything, 0 if no items were added (as will be the case if the dot
   * is at the far right of the production or the symbol to the right of the
   * dot is a terminal).
   */
  
  int did_something = 0;
  int rhs_is_nullable;
  PRODUCTION *prod;
  ITEM *close_item;
  SET *closure_set;
  SYMBOL **symp;

  if (!item->right_of_dot) {
    return 0;
  }

  if (!ISNONTERM(item->right_of_dot)) {
    return 0;
  }

  closure_set = newset();

  /* the symbol to the right of the dot is a nonterminal. do the following:
   *
   *(1) for (every production attached to that nonterminal)
   *(2)	if (the current production is not already in the set of closure items)
   *(3)   add it;
   *(4) if (the d in [A->b.Cd, e] doesn't exist)
   *(5)   add e to the lookaheads in the closure production.
   *	  else
   *(6)   the d in [A->b.Cd, e] does exist, compute FIRST(de) and add
   *		  it to the lookaheads for the current item if necessary.
   */

  for (prod = item->right_of_dot->productions; prod; prod = prod->next) { /* (1) */
    if (!(close_item = in_closure_items(prod, closure_items, *nitems))) { /* (2) */
      if (--(*maxitems) < 0) {
        error(FATAL, "LR(1) closure set too large\n");
      }
      closure_items[(*nitems)++] = close_item = newitem(prod);  /* (3) */
      ++did_something;
    }

    if (!*(symp = &(item->prod->rhs[item->dot_posn + 1]))) {    /* (4) */
      did_something |= add_lookahead(close_item->lookaheads, item->lookaheads);   /* (5) */
    } else {
      truncate(closure_set); /* (6) */
      rhs_is_nullable = first_rhs(closure_set, symp, item->prod->rhs_len - item->dot_posn - 1);
      REMOVE(closure_set, EPSILON);
      if (rhs_is_nullable) {
        UNION(closure_set, item->lookaheads);
      }

      did_something |= add_lookahead(close_item->lookaheads, closure_set);
    }
  }
  
  delset(closure_set);
  return did_something;
}

static int closure(STATE *kernel, ITEM *closure_items[], int maxitems)
{
  /* kernel: kernel state to close
   * closure_items: array into which closure items are put
   * maxitems: size of the closure_items[] array
   */
  
  /* do LR(1) closure on the kernel items array in the input STATE. when
   * finished, closure_items[] will hold the new items. the logic is:
   *
   * (1) for (each kernel item)
   *         do LR(1) closure on that item
   * (2) while (items were added in the previous step or are added below)
   *         do LR(1) closure on the items that were added
   */

  int i;
  int nclose = 0; /* number of closure items */
  int did_something = 0;
  ITEM **p = kernel->kernel_items;

  for (i = kernel->nkitems; --i >= 0; ) {   /* (1) */
    did_something |= do_close(*p++, closure_items, &nclose, &maxitems);
  }

  while (did_something) { /* (2) */
    did_something = 0;
    p = closure_items;
    for (i = nclose; --i >= 0;) {
      did_something |= do_close(*p++, closure_items, &nclose, &maxitems);
    }
  }

  return nclose;
}

static void reduce_one_item(STATE *state, ITEM *item)
{
  /* item: reduce on this item 
   * state: from this state
   */
  
  int token;      /* current lookahead */
  int pprec;      /* precedence of production */
  int tprec;      /* precedence of token */
  int assoc;      /* associativity of token */
  int reduce_by;
  int resolved;   /* true if conflict can be resolved */
  ACT *ap;

  if (item->right_of_dot) { /* no reduction required */
    return;
  }

  pprec = item->prod->prec; /* precedence of entire production */
  
  if (Verbose > 1) {
    printf("ITEM: %s\n", stritem(item, 1));
  }

  for (next_member(NULL); (token = next_member(item->lookaheads)) >= 0; ) {
    tprec = Precedence[token].level;  /* precedence of lookahead symbol */
    assoc = Precedence[token].assoc;

    if (Verbose > 1) {
      printf("TOKEN: %s (prec=%d, assoc=%c)\n", Terms[token]->name, tprec, assoc);
    }

    if (!(ap = p_action(state->num, token))) { /* no conflicts */
      add_action(state->num, token, -(item->prod_num));
      if (Verbose > 1) {
        printf("Action[%d][%s]=%d\n", state->num, Terms[token]->name, -(item->prod_num));
      }
    } else if (ap->do_this <= 0) {
      /* resolve a reduce/reduce conflict in favor of the production with the smaller number
       * print a warning
       */
      ++Reduce_reduce;
      reduce_by = min(-(ap->do_this), item->prod_num);

      error(WARNING, "state %2d: reduce/reduce conflict ", state->num);
      error(NOHDR, "%d/%d on %s (choose %d)\n", -(ap->do_this), 
            item->prod_num, token ? Terms[token]->name : "<_EOI_>", reduce_by);
      ap->do_this = -reduce_by;
    } else {
      /* shift/reduce conflict */
      if (resolved = (pprec && tprec)) {
        if (tprec < pprec || (pprec == tprec && assoc == 'l')) {
          ap->do_this = -(item->prod_num);
        }
      }

      if (Verbose > 1 || !resolved) {
        ++Shift_reduce;
        error(WARNING,"state %2d: shift/reduce conflict ", state->num);
        error(NOHDR, "%s/%d (choose %s) %s\n", Terms[token]->name, item->prod_num,
              ap->do_this < 0 ? "reduce" : "shift", 
              resolved ? "(resolved)" : "");
      }
    }
  }
}

static void addreductions(STATE *state, void *junk)
{
  /* this routine is called for each state. it adds the reductions using the
   * disambiguating rules described in the text, and then prints the state to
   * yyout.doc if Verbose is true
   */
  
  int i;
  ITEM **item_p;

  if (Verbose > 1) {
    printf("----------------------------------\n");
    pstate_stdout(state);
    printf("----------------------------------\n");
  }

  for (i = state->nkitems, item_p = state->kernel_items; --i >= 0; ++item_p) {
    reduce_one_item(state, *item_p);
  }

  for (i = state->neitems, item_p = state->epsilon_items; --i >= 0; ++item_p) {
    reduce_one_item(state, *item_p);
  }

  if (Verbose) {
    pstate(state);
  }
}

static void reductions()
{
  /* do the reductions. if there's memory, sort the table by state number
   * first so that yyout.doc will look nice
   */
  
  Sort_by_number = 1;
  if (!ptab(States, (ptab_t)addreductions, NULL, 1)) {
    ptab(States, (ptab_t)addreductions, NULL, 0);
  }
}

static void make_yy_lhs(PRODUCTION **prodtab)
{
  static char *text[] = {
	  "the Yy_lhs array is used for reductions. it is indexed by production",
	  "number and holds the associated left-hand side adjusted so that the",
	  "number can be used as an index into Yy_goto.",
	  NULL
  };

  PRODUCTION *prod;
  int i;

  comment(Output, text);
  output("YYPRIVATE int Yy_lhs[%d] = \n{\n", Num_productions);
  
  for (i = 0; i < Num_productions; ++i) {
    prod = *prodtab++;
    output("\t/* %3d */\t%d", prod->num, ADJ_VAL(prod->lhs->val));

    if (i != Num_productions - 1) {
      output(",");
    }

    if (i % 3 == 2 || i == Num_productions - 1) { /* use three columns */
      output("\n");
    }
  }
  output("};\n");
}

static void make_yy_reduce(PRODUCTION **prodtab)
{
  static char *text[] = {
	  "the Yy_reduce[] array is indexed by production number and holds",
	  "the number of symbols on the right-hand side of the production",
	  NULL
  };

  PRODUCTION *prod;
  int i;

  comment(Output, text);
  output("YYPRIVATE int Yy_reduce[%d] = \n{\n", Num_productions);
  
  for (i = 0; i < Num_productions; i++) {
    prod = *prodtab++;
    output("\t/* %3d */\t%d", prod->num, prod->rhs_len);

    if (i != Num_productions - 1) {
      output(",");
    }

    if (i % 3 == 2 || i == Num_productions - 1) { /* use three columns */
      output("\n");
    }
  }

  output("};\n");
}


static void mkprod(SYMBOL *sym, PRODUCTION **prodtab)
{
  PRODUCTION *p;
  if (ISNONTERM(sym)) {
    for (p = sym->productions; p; p = p->next) {
      if (p->num >= Num_productions) {
        error(FATAL, "mkprod bad prod num: %d\n", p->num);
      }

      prodtab[p->num] = p;
    }
  }

}

/*
 * the following routines generate compressed parse tables 
 * the default transition is the error transition
 */

static void print_reductions()
{
  /* output the various tables needed to do reductions */
  PRODUCTION **prodtab;

  if (!(prodtab = (PRODUCTION**) malloc (sizeof(PRODUCTION *) * Num_productions))) {
    error(FATAL, "no memory to output LALR(1) reduction tables\n");
  } else {
    ptab(Symtab, (ptab_t)mkprod, prodtab, 0);
  }

  make_yy_lhs(prodtab);
  make_yy_reduce(prodtab);

  free(prodtab);
}

static void print_tab(ACT **table, char *row_name, char *col_name, int make_private)
{
  /*
   * row_name: name to use for row arrays;
   * col_name: name to use for the row-pointers array
   * make_private: make index table private (rows always private)
   /
  
  /* output the action and goto table */

  int i, j;
  ACT *ele, **elep; /* table element and pointer to same */
  ACT *e, **p;
  int count;        /* # of transitions from this state, always > 0 */
  int column;
  SET *redundant = newset();  /* mark redundant rows */


  static char	*act_text[] = {
	  "the Yy_action table is action part of the LALR(1) transition",
	  "matrix. it's compressed and can be accessed using the yy_next()",
	  "subroutine, declared below",
	  "",
	  "             Yya000[]={   3,   5,3   ,   2,2   ,   1,1   };",
	  "  state number---+        |    | |",
	  "  number of pairs in list-+    | |",
	  "  input symbol (terminal)------+ |",
	  "  action-------------------------+",
	  "",
	  "  action = yy_next(Yy_action, cur_state, lookahead_symbol);",
	  "",
	  "	action <  0   -- reduce by production n,  n == -action",
	  "	action == 0   -- accept. (ie. reduce by production 0)",
	  "	action >  0   -- shift to state n,  n == action",
	  "	action == YYF -- error",
	  NULL
  };

  static char	*goto_text[] = {
	  "the Yy_goto table is goto part of the LALR(1) transition matrix",
	  "",
	  " nonterminal = Yy_lhs[ production number by which we just reduced ]",
	  "",
	  "              Yyg000[]={   3,   5,3   ,   2,2   ,   1,1   };",
	  "  uncovered state-+        |    | |",
	  "  number of pairs in list--+    | |",
	  "  nonterminal-------------------+ |",
	  "  goto this state-----------------+",
	  "",
	  "it's compressed and can be accessed using the yy_next() subroutine,",
	  "declared below, like this:",
	  "",
	  "  goto_state = yy_next(Yy_goto, cur_state, nonterminal);",
	  NULL
  };

  comment(Output, table == Actions ? act_text : goto_text);

  /*
   * modify the matrix so that, if a duplicate rows exists, only one
   * copy of it is kept around. the extra rows are marked as such by setting
   * a bit in the "redundant" set. (the memory used for the chains is just
   * discarded) the redundant table element is made to point at the row
   * that it duplicates
   */

  
  for (elep = table, i = 0; i < Nstates; ++elep, i++) {
    if (MEMBER(redundant, i)) {
      continue;
    }

    for (p = elep + 1, j = i + 1; j < Nstates; ++p, ++j) {
      if (MEMBER(redundant, j)) {
        continue;
      }

      ele = *elep;    /* pointer to template chain */
      e = *p;         /* chain to compare against template */
      if (!e || !ele) { /* either or both strings have no elements */
        continue;
      }

      for (; ele && e; ele = ele->next, e = e->next) {
        if ((ele->do_this != e->do_this) || (ele->sym != e->sym)) {
          break;
        }
      }

      if (!e && !ele) {
        /* then the chains are the same. mark the chain being compared
		     * as redundant, and modify table[j] to hold a pointer to the
		     * template pointer
		     */
        ADD(redundant, j);
        table[j] = (ACT *) elep;
      }
    }
  }

  /* output the row arrays */

  for (elep = table, i = 0; i < Nstates; ++elep, i++) {
    if (!*elep || MEMBER(redundant, i)) {
      continue;
    }

    /* count the number of transitions from this state */

    count = 0;
    for (ele = *elep; ele; ele = ele->next) {
      count++;
    }

    output("YYPRIVATE YY_TTYPE %s%03d[] = {%2d, ", row_name, (int) (elep - table), count);

    column = 0;
    for (ele = *elep; ele; ele = ele->next) {
      ++Npairs;
      output("%3d,%d", ele->sym, ele->do_this);

      if (++column != count) {
        outc(',');
      }
      if (column % 5 == 0) {
        output("\n\t\t\t\t  ");
      }
    }
    output("};\n");
  }

  if (make_private) {
    output("\nYYPRIVATE YY_TTYPE *%s[%d] = \n", col_name, Nstates);
  } else {
    output("\nYY_TTYPE *%s[%d] = \n", col_name, Nstates);
  }

  output("{");

  for (elep = table, i = 0; i < Nstates; i++, elep++) {
    if (i == 0 || (i % 8) == 0) {
      output("\n/* %3d */ ", i);
    }
    if (MEMBER(redundant, i)) {
      output("%s%03d", row_name, (int)(((ACT**)(*elep)) - table));
    } else {
      output(*elep ? "%s%03d" : " NULL", row_name, i);
    }

    if (i != Nstates - 1) {
      output(", ");
    }
  }
  output("\n};\n");
  delset(redundant);
}

static int lr(STATE *cur_state)
{
  /* make LALR(1) state machine. the shifts and gotos are done here, the
   * reductions are done elsewhere. return the number of states.
   */
  
  ITEM **p;
  ITEM **first_item;
  ITEM *closure_items[MAXCLOSE];
  STATE *next;  /* next state */
  int isnew;    /* next state is a new state */
  int nclose;   /* number of items in closure_items */
  int nitems;   /* # items with same symbol to right of dot */

  int val;      /* value of symbol to right of dot */
  SYMBOL *sym;  /* actual symbol to right of dot */

  int nlr = 0;  /* Nstates + nlr = number of LR(1) states */

  add_unfinished(cur_state);
  
  while (cur_state = get_unfinished()) {
    if (Verbose > 1) {
      printf("next pass.. working on state %d\n", cur_state->num);
    }

    /* closure()  adds normal closure items to closure_items array
	   * kclose()   adds to that set all items in the kernel that have
	   *            outgoing transitions (ie. whose dots aren't at the far
	   *	          right)

	   * qsort()   sorts the closure items by the symbol to the right
	   *	         of the dot. epsilon transitions will sort to the head of
	   *	         the list, followed by transitions on nonterminals,
	   *	         followed by transitions on terminals
	   * move_eps() moves the epsilon transitions into the closure kernel set.
	   *            it returns the number of items that it moved
	   */
    nclose = closure(cur_state, closure_items, MAXCLOSE);
    nclose = kclosure(cur_state, closure_items, MAXCLOSE, nclose);
    if (nclose == 0) {
      if (Verbose > 1) {
        printf("there were no closure items added\n");
      }
    } else {
      qsort(closure_items, nclose, sizeof(ITEM *), item_cmp);
      nitems = move_eps(cur_state, closure_items, nclose);
      p = closure_items + nitems;
      nclose = nclose - nitems;
      
      if (Verbose > 1) {
        pclosure(cur_state, p, nclose);
      }
    }

    /* all of the remaining items have at least one symbol to the	right of the dot */
    while (nclose > 0) { /* fails immediatly if no closure items */
      first_item = p;
      sym = (*first_item)->right_of_dot;
      val = sym->val;

      /* collect all items with the same symbol to the right of the dot 
	     * on exiting the loop, nitems will hold the number of these items
	     * and p will point at the first nonmatching item. finally nclose is
	     * decremented by nitems. items = 0
	     */
      nitems = 0;
      do {
        movedot(*p++);
        ++nitems;
      } while (--nclose > 0 && RIGHT_OF_DOT(*p) == val);

      /* (1) newstate() gets the next state. it returns NEW if the state
	     *	   didn't exist previously, CLOSED if LR(0) closure has been
	     *     performed on the state, UNCLOSED otherwise.
	     * (2) add a transition from the current state to the next state.
	     * (3) if it's a brand-new state, add it to the unfinished list.
	     * (4) otherwise merge the lookaheads created by the current closure
	     *     operation with the ones already in the state.
	     * (5) if the merge operation added lookaheads to the existing set,
	     *     add it to the unfinished list.
	     */
      
      isnew = newstate(first_item, nitems, &next); /* (1) */
      if (!cur_state->closed) { /* (2) */
        if (ISTERM(sym)) {
          add_action(cur_state->num, val, next->num);
        } else {
          add_goto(cur_state->num, val, next->num);
        }
      }
      
      if (isnew == NEW) { 
        add_unfinished(next); /* (3) */
      } else {
        if (merge_lookaheads(next->kernel_items, first_item, nitems)) { /* (4) */
          add_unfinished(next); /* (5) */
          ++nlr;
        }
        while (--nitems >= 0) {
          freeitem(*first_item++);
        }
      }
      
      fprintf(stderr, "\rLR:%-3d LALR:%-3d", Nstates + nlr, Nstates);
    }
    cur_state->closed = 1;
  }

  free_recycled_items();
  if (Verbose) {
    fprintf(stderr, "states, %d items, %d shift and goto transitions\n", Nitems, Ntab_entries);
  }

  return Nstates;
}


static int state_cmp(STATE *new, STATE *tab_node)
{
  /* new: pointer to new node (ignored if Sort_by_number is false)
   * tab_node: pointer to existing node
   */
  
  /* compare two states as described in the text. return a number representing
   * the relative weight of the states, or 0 of the states are equivalent.
   */
  
  ITEM **tab_item; /* array of items for existing state */
  ITEM **item;     /* array of items for new state */
  int nitem;
  int cmp;

  if (Sort_by_number) {
    return (new->num - tab_node->num);
  }


  if (cmp = State_nitems - tab_node->nkitems) { /* state with largest number of items is larger */
    return cmp;
  }

  nitem = State_nitems;
  item = State_items;

  tab_item = tab_node->kernel_items;

  for (; --nitem >= 0; ++tab_item, ++item) {
    if (cmp = (*item)->prod_num - (*tab_item)->prod_num) {
      return cmp;
    }

    if (cmp = (*item)->dot_posn - (*tab_item)->dot_posn) {
      return cmp;
    }
  }

  return 0; /* states are equivalent */
}


static unsigned int state_hash(STATE *sym)
{
  /* hash function for STATEs. mum together production numbers and dot
   * positions of the kernel items.
   */
  ITEM **items;   /* array of items for new state */
  int nitems;     /* size of */

  unsigned int total;

  items = State_items;
  nitems = State_nitems;
  total = 0;

  for (; --nitems >= 0; ++items) {
    total += (*items)->prod_num + (*items)->dot_posn; 
  }

  return total;
}

void make_parse_tables()
{
  /* prints an LALR(1) transition matrix for the grammar currently
   * represented in the symbol table
   */
  
  ITEM *item;
  STATE *state;
  PRODUCTION *start_prod;
  FILE *fp, *old_output;

  /* make data structures used to produce the table, and create an initial
   * LR(1) item containing the start production and the end-of-input marker
   * as a lookahead symbol.
   */
  
  States = maketab(257, state_hash, state_cmp);
  if (!Goal_symbol) {
    error(FATAL, "no goal symbol\n");
  }

  start_prod = Goal_symbol->productions;
  
  if (start_prod->next) {
    error(FATAL, "start symbol must have only one right-hand side\n");
  }

  item = newitem(start_prod);         /* make item for start production */
  ADD(item->lookaheads, _EOI_);       /* end-of-input marker as a lookahead symbol */
  ADD(item->lookaheads, WHITESPACE);  /* whitespace marker as a lookahead symbol */
  
  newstate(&item, 1, &state);
  if (lr(state)) {  /* add shifts and gotos to the table */
    if (Verbose) {
      printf("adding reductions:\n");
    }
    
    reductions(); /* add the reductions */
    
    if (Verbose) {
      printf("creating tables:\n");
    }

    if (!Make_yyoutab) {  /* tables go in yyout.c */
      print_tab(Actions, "Yya", "Yy_action", 1);
      print_tab(Gotos, "Yyg", "Yy_goto", 1);
    } else { /* tables go in yyoutab.c*/

      if (!(fp = fopen(TAB_FILE, "w"))) {
        error(NONFATAL, "can't open %s ignoring -T\n", TAB_FILE);
        print_tab(Actions, "Yya", "Yy_action", 1);
        print_tab(Gotos, "Yyg", "Yy_goto", 1);
      } else {
        output("extern YY_TTYPE *Yy_action[]; /* in yyoutab.c */\n");
        output("extern YY_TTYPE *Yy_goto[];   /* in yyoutab.c */\n");
        old_output = Output;
        Output = fp;
        fprintf(fp, "#include <stdio.h>\n");
        fprintf(fp, "typedef short YY_TTYPE;\n");
        fprintf(fp, "#define YYPRIVATE %s\n", Public ? "/* empty */" : "static");

        print_tab(Actions, "Yya", "Yy_action", 0);
        print_tab(Gotos, "Yyg", "Yy_goto", 0);
        fclose(fp);
        Output = old_output;
      }
    }
    print_reductions();
  }
}

void lr_stats(FILE *fp)
{
  /*  print out various statistics about the table-making process */
  fprintf(fp, "%4d  LALR(1) states\n", Nstates);
  fprintf(fp, "%4d  items\n", Nitems);
  fprintf(fp, "%4d  nonerror transitions in tables\n", Ntab_entries);
  fprintf(fp, "%4ld/%-4d unfinished items\n", (long)(Next_allocate - Heap), MAX_UNFINISHED);

  fprintf(fp, "%4d bytes required for LALR(1) transition matrix\n", 
        (2 * sizeof(int*) * Nstates)   /* index arrays */ 
        + Nstates                      /* count fields */
        + (Npairs *sizeof(short)));    /* pairs */
  
  fprintf(fp, "\n");
}


int lr_conflicts(FILE *fp)
{
  /* print out statistics for the inadequate states and return the number of
   * conflicts.
   */
  
  fprintf(fp, "%4d  shift/reduce conflicts\n", Shift_reduce);
  fprintf(fp, "%4d  reduce/reduce conflicts\n", Reduce_reduce);

  return Shift_reduce + Reduce_reduce; 
}