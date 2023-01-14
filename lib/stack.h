#include <stdbool.h>

void stack_init(unsigned size);
void stack_destroy();

void stack_push(int t);
int stack_pop();

bool stack_is_empty();
