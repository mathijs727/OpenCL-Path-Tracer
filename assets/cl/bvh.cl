#ifndef __BVH_CL
#define __BVH_CL
#include "ray.cl"

typedef struct
{
	float centre[3];
	float extents[3];
	union
	{
		unsigned int leftChildIndex;
		unsigned int firstTriangleIndex;
	};
	unsigned int triangleCount;
} ThinBvhNodeSerialized;

typedef struct
{
	float3 min;
	float3 max;
	union
	{
		unsigned int leftChildIndex;
		unsigned int firstTriangleIndex;
	};
	unsigned int triangleCount;
} ThinBvhNode;

void loadThinBvhNode(const __global ThinBvhNodeSerialized* serialized, ThinBvhNode* out)
{
	float3 centre = vload3(0, serialized->centre);
	float3 extents = vload3(0, serialized->extents);
	out->min = centre - extents;
	out->max = centre + extents;
	out->leftChildIndex = serialized->leftChildIndex;
	out->triangleCount = serialized->triangleCount;
}

// https://tavianator.com/fast-branchless-raybounding-box-intersections/
bool intersectRayThinBvh(const Ray* ray, const ThinBvhNode* node)
{
	float tmin = -INFINITY, tmax = INFINITY;

	if (ray->direction.x != 0.0f)
	{
		float tx1 = (node->min.x - ray->origin.x) / ray->direction.x;
		float tx2 = (node->max.x - ray->origin.x) / ray->direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (ray->direction.y != 0.0f)
	{
		float ty1 = (node->min.y - ray->origin.y) / ray->direction.y;
		float ty2 = (node->max.y - ray->origin.y) / ray->direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (ray->direction.z != 0.0f)
	{
		float tz1 = (node->min.z - ray->origin.z) / ray->direction.z;
		float tz2 = (node->max.z - ray->origin.z) / ray->direction.z;

		tmin = max(tmin, min(tz1, tz2));
		tmax = min(tmax, max(tz1, tz2));
	}

	// tmax >= 0: prevent boxes before the starting position from being hit
	// See the comment section at:
	//  https://tavianator.com/fast-branchless-raybounding-box-intersections/
	return tmax >= tmin && tmax >= 0;
}

bool intersectLineThinBvh(const Line* line, const ThinBvhNode* node)
{
	float tmin = -INFINITY, tmax = INFINITY;

	float3 direction = normalize(line->dest - line->origin);
	if (direction.x != 0.0f)
	{
		float tx1 = (node->min.x - line->origin.x) / direction.x;
		float tx2 = (node->max.x - line->origin.x) / direction.x;

		tmin = max(tmin, min(tx1, tx2));
		tmax = min(tmax, max(tx1, tx2));
	}

	if (direction.y != 0.0f)
	{
		float ty1 = (node->min.y - line->origin.y) / direction.y;
		float ty2 = (node->max.y - line->origin.y) / direction.y;

		tmin = max(tmin, min(ty1, ty2));
		tmax = min(tmax, max(ty1, ty2));
	}

	if (direction.z != 0.0f)
	{
		float tz1 = (node->min.z - line->origin.z) / direction.z;
		float tz2 = (node->max.z - line->origin.z) / direction.z;

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
	float pathLen = dot(path, path);
	return ( tmax >= tmin && tmax >= 0 && (tmin < 0 || tmin*tmin < pathLen) );
}

#endif// __BVH_CL 