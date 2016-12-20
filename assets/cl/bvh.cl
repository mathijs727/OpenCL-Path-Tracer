#ifndef __BVH_CL
#define __BVH_CL
#include "ray.cl"

typedef struct
{
	float3 centre;
	float3 extents;
	union
	{
		unsigned int leftChildIndex;
		unsigned int firstTriangleIndex;
	};
	unsigned int triangleCount;
} ThinBvhNode;

typedef struct
{
	float3 centre;
	float3 extents;

	float invTransform[16];
	union
	{
		struct
		{
			unsigned int leftChildIndex;
			unsigned int rightChildIndex;
		};
		unsigned int thinBvh;
	};
	unsigned int isLeaf;
} FatBvhNode;

bool intersectRayFatBvh(const Ray* ray, const __global FatBvhNode* node, float nearestT)
{
	float tmin = -INFINITY, tmax = INFINITY;
	float3 aabbMin = node->centre - node->extents;
	float3 aabbMax = node->centre + node->extents;

	if (ray->direction.x != 0.0f)
	{
		float tx1 = (aabbMin.x - ray->origin.x) / ray->direction.x;
		float tx2 = (aabbMax.x - ray->origin.x) / ray->direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (ray->direction.y != 0.0f)
	{
		float ty1 = (aabbMin.y - ray->origin.y) / ray->direction.y;
		float ty2 = (aabbMax.y - ray->origin.y) / ray->direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (ray->direction.z != 0.0f)
	{
		float tz1 = (aabbMin.z - ray->origin.z) / ray->direction.z;
		float tz2 = (aabbMax.z - ray->origin.z) / ray->direction.z;

		tmin = max(tmin, min(tz1, tz2));
		tmax = min(tmax, max(tz1, tz2));
	}

	// tmax >= 0: prevent boxes before the starting position from being hit
	// See the comment section at:
	//  https://tavianator.com/fast-branchless-raybounding-box-intersections/
	return tmax >= tmin && tmax >= 0 && tmin < nearestT;
}

bool intersectLineFatBvh(const Line* line, const FatBvhNode* node)
{
	float tmin = -INFINITY, tmax = INFINITY;
	float3 aabbMin = node->centre - node->extents;
	float3 aabbMax = node->centre + node->extents;

	float3 direction = normalize(line->dest - line->origin);
	if (direction.x != 0.0f)
	{
		float tx1 = (aabbMin.x - line->origin.x) / direction.x;
		float tx2 = (aabbMax.x - line->origin.x) / direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (direction.y != 0.0f)
	{
		float ty1 = (aabbMin.y - line->origin.y) / direction.y;
		float ty2 = (aabbMax.y - line->origin.y) / direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (direction.z != 0.0f)
	{
		float tz1 = (aabbMin.z - line->origin.z) / direction.z;
		float tz2 = (aabbMax.z - line->origin.z) / direction.z;

		tmin = max(tmin, min(tz1, tz2));
		tmax = min(tmax, max(tz1, tz2));
	}

	// tmax >= 0: prevent boxes before the starting position from being hit
	// See the comment section at:
	//  https://tavianator.com/fast-branchless-raybounding-box-intersections/
	// Additionally, if tmin < 0:
	//  - line starts inside of box, return true
	// Otherwise (tmin >= 0):
	//  - line starts outside of the box
	//  - check if the line reaches tmin (where it enters the box)
	float3 path = line->dest - line->origin;
	float pathLen2 = dot(path, path);
	return ( tmax >= tmin && tmax >= 0 && (tmin < 0 || tmin*tmin < pathLen2) );
}

// https://tavianator.com/fast-branchless-raybounding-box-intersections/
bool intersectRayThinBvh(const Ray* ray, const ThinBvhNode* node, float nearestT)
{
	float tmin = -INFINITY, tmax = INFINITY;
	float3 aabbMin = node->centre - node->extents;
	float3 aabbMax = node->centre + node->extents;

	if (ray->direction.x != 0.0f)
	{
		float tx1 = (aabbMin.x - ray->origin.x) / ray->direction.x;
		float tx2 = (aabbMax.x - ray->origin.x) / ray->direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (ray->direction.y != 0.0f)
	{
		float ty1 = (aabbMin.y - ray->origin.y) / ray->direction.y;
		float ty2 = (aabbMax.y - ray->origin.y) / ray->direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (ray->direction.z != 0.0f)
	{
		float tz1 = (aabbMin.z - ray->origin.z) / ray->direction.z;
		float tz2 = (aabbMax.z - ray->origin.z) / ray->direction.z;

		tmin = max(tmin, min(tz1, tz2));
		tmax = min(tmax, max(tz1, tz2));
	}

	// tmax >= 0: prevent boxes before the starting position from being hit
	// See the comment section at:
	//  https://tavianator.com/fast-branchless-raybounding-box-intersections/
	return tmax >= tmin && tmax >= 0 && tmin < nearestT;
}

bool intersectLineThinBvh(const Line* line, const ThinBvhNode* node)
{
	float tmin = -INFINITY, tmax = INFINITY;
	float3 aabbMin = node->centre - node->extents;
	float3 aabbMax = node->centre + node->extents;

	float3 direction = normalize(line->dest - line->origin);
	if (direction.x != 0.0f)
	{
		float tx1 = (aabbMin.x - line->origin.x) / direction.x;
		float tx2 = (aabbMax.x - line->origin.x) / direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (direction.y != 0.0f)
	{
		float ty1 = (aabbMin.y - line->origin.y) / direction.y;
		float ty2 = (aabbMax.y - line->origin.y) / direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (direction.z != 0.0f)
	{
		float tz1 = (aabbMin.z - line->origin.z) / direction.z;
		float tz2 = (aabbMax.z - line->origin.z) / direction.z;

		tmin = max(tmin, min(tz1, tz2));
		tmax = min(tmax, max(tz1, tz2));
	}

	// tmax >= 0: prevent boxes before the starting position from being hit
	// See the comment section at:
	//  https://tavianator.com/fast-branchless-raybounding-box-intersections/
	// Additionally, if tmin < 0:
	//  - line starts inside of box, return true
	// Otherwise (tmin >= 0):
	//  - line starts outside of the box
	//  - check if the line reaches tmin (where it enters the box)
	float3 path = line->dest - line->origin;
	float pathLen2 = dot(path, path);
	return ( tmax >= tmin && tmax >= 0 && (tmin < 0 || tmin*tmin < pathLen2) );
}

#endif// __BVH_CL 