#include "stack.h"
#include <stdlib.h>

#include "stack.h"

static int *stack;
static int last;

void stack_init(unsigned size)
{
	stack = malloc(size * sizeof(int));
	last = 0;
}

void stack_destroy() { free(stack); }


void stack_push(int t) { stack[last++] = t; }

int stack_pop() { return stack[--last]; }


bool stack_is_empty() { return last == 0 ? true : false; }
