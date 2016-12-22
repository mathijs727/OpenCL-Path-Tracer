#pragma once
#include "aabb.h"

namespace raytracer
{
	struct TopBvhNode
	{
		AABB bounds;
		glm::mat4 invTransform;
		union
		{
			struct
			{
				u32 leftChildIndex;
				u32 rightChildIndex;
			};
			u32 subBvhNode;
		};
		u32 isLeaf;
		u32 __padding_cl;// Make the struct 16 byte aligned
	};

	struct SubBvhNode
	{
		AABB bounds;
		union
		{
			u32 leftChildIndex;
			u32 firstTriangleIndex;
		};
		u32 triangleCount;
		u32 __padding_cl[2];

		SubBvhNode()
		{
			leftChildIndex = 0;
			triangleCount = 0;
		}
	};
}