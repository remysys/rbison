#ifndef _STACK_H
#define _STACK_H
/*
 * stack.h  stack-maintenance macros. creates downward-growing stacks
 */

#define stack_cls   /* empty */

#define stack_dcl(stack, type, size) typedef type t_##stack; \
              stack_cls t_##stack stack[size];    \
              stack_cls t_##stack (*p_##stack)    \
                    = stack + (size)

#define stack_clear(stack) ((p_##stack) = (stack + sizeof(stack)/sizeof(*stack)))
#define stack_full(stack) ((p_##stack) <= stack)
#define stack_empty(stack) ((p_##stack) >= (stack + sizeof(stack)/sizeof(*stack)))
#define stack_ele(stack) ((sizeof(stack)/sizeof(*stack)) - (p_##stack - stack))
#define stack_item(stack, offset) (*(p_##stack + (offset)))
#define stack_p(stack)  p_##stack
#define push_(stack, x) (*--p_##stack = (x))
#define pop_(stack) (*p_##stack++)
#define push(stack, x) (stack_full(stack) ? ((t_##stack)(long)(stack_err(1))) : push_(stack, x))
#define pop(stack) (stack_empty(stack) ? ((t_##stack)(long)(stack_err(0))) : pop_(stack))
#define popn_(stack, amt) ((p_##stack) += amt)[-amt])
#define popn(stack, amt) ((stack_ele(stack) < amt) ? ((t_##stack)(long)(stack_err(0))) : popn_(stack, amt))
#define stack_err(overflow) ((overflow) ? ferr("stack overflow\n") : ferr("stack underflow\n"))
#endif

