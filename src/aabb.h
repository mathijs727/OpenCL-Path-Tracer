#pragma once
#include <glm.hpp>
#include "types.h"
#include "template/includes.h"

namespace raytracer {
	struct AABB
	{
		union
		{
			glm::vec3 min;
			cl_float3 __min_cl;
		};
		union
		{
			glm::vec3 max;
			cl_float3 __max_cl;
		};

		AABB() {
			min = glm::vec3(std::numeric_limits<float>::max());
			max = glm::vec3(std::numeric_limits<float>::lowest());
		};
		AABB(const AABB& other)
		{
			memcpy(this, &other, sizeof(AABB));
		}
		AABB& operator=(const AABB& other)
		{
			memcpy(this, &other, sizeof(AABB));
			return *this;
		}

		void fit(const AABB& other)
		{
			min = glm::min(min, other.min);
			max = glm::max(max, other.max);
		}

		float surfaceArea() {
			glm::vec3 size = max - min;
			return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
		}
	};
}