#ifndef __STACK_CL
#define __STACK_CL
#include "ray.cl"

#define MAX_STACK_SIZE 8

typedef struct
{
	float3 multiplier;
	bool lastSpecular;
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

void StackPush(
	Stack* stack,
	float3 origin,
	float3 direction,
	float3 multiplier)
{
	// If we push to a full stack, ignore the operation
	if (stack->numItems == MAX_STACK_SIZE)
		return;

	StackItem* item = &stack->items[stack->numItems++];
	item->ray.origin = origin;
	item->ray.direction = direction;
	item->multiplier = multiplier;
	item->lastSpecular = true;
}


void StackPushNEE(
	Stack* stack,
	float3 origin,
	float3 direction,
	float3 multiplier,
	bool lastSpecular)
{
	// If we push to a full stack, ignore the operation
	if (stack->numItems == MAX_STACK_SIZE)
		return;

	StackItem* item = &stack->items[stack->numItems++];
	item->ray.origin = origin;
	item->ray.direction = direction;
	item->multiplier = multiplier;
	item->lastSpecular = lastSpecular;
}

StackItem StackPop(Stack* stack)
{
	return stack->items[--stack->numItems];
}

bool StackEmpty(const Stack* stack)
{
	return stack->numItems == 0;
}

#endif// __STACK_CL