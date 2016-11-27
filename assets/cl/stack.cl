#ifndef __STACK_CL
#define __STACK_CL
#include "ray.cl"

#define MAX_STACK_SIZE 16

typedef struct
{
	float3 multiplier;
	Ray ray;
} StackItem;

typedef struct
{
	StackItem items[MAX_STACK_SIZE];
	int numItems;
} Stack;

void StackInit(Stack* stack)
{
	stack->numItems = 0;
}

void StackPush(Stack* stack, const StackItem* item)
{
	// If we push to a full stack, ignore the operation
	if (stack->numItems == MAX_STACK_SIZE)
		return;

	stack->items[stack->numItems++] = *item;
}

void StackPop(Stack* stack, StackItem* out)
{
	*out = stack->items[--stack->numItems];
}

bool StackEmpty(const Stack* stack)
{
	return stack->numItems == 0;
}

#endif// __STACK_CL