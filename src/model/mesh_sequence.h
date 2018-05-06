#pragma once
#include "types.h"
#include "transform.h"
#include "vertices.h"
#include "imesh.h"
#include "bvh/bvh_nodes.h"
#include <vector>

struct aiMesh;
struct aiScene;

namespace raytracer
{
	class MeshSequence : public IMesh
	{
	public:
		MeshSequence() : _current_frame(0), _refitting(false), _bvh_needs_update(true) { };
		~MeshSequence() { };

		void loadFromFiles(const char* fileFormat, bool refitting = false, const Transform& offset = Transform());
		void goToNextFrame();

		const std::vector<VertexSceneData>& getVertices() const override { return _frames[_current_frame].vertices; }
		const std::vector<TriangleSceneData>& getTriangles() const override { return _frames[_current_frame].triangles; }
		const std::vector<Material>& getMaterials() const override { return _frames[_current_frame].materials; }
		const std::vector<SubBvhNode>& getBvhNodes() const override { return _bvh_nodes; }

	private:
		// Cannot directly return std::vector<uint32_t> because then we would have to reference stack memory
		std::vector<uint32_t> _emmisive_triangles_dummy;
		const std::vector<uint32_t>& getEmissiveTriangles() const override { return _emmisive_triangles_dummy; }// Not implemented yet
	public:
		uint32_t getBvhRootNode() const override { return _bvh_root_node; };

		bool isDynamic() const override { return true; };
		uint32_t maxNumVertices() const override;
		uint32_t maxNumTriangles() const override;
		uint32_t maxNumMaterials() const override;
		uint32_t maxNumBvhNodes() const override;
		void buildBvh() override;
	private:
		void addSubMesh(const aiScene* scene,
			unsigned mesh_index,
			const glm::mat4& transform_matrix,
			std::vector<VertexSceneData>& vertices,
			std::vector<TriangleSceneData>& triangles,
			std::vector<Material>& materials,
			const char* texturePath);
		void loadFile(const char* file, const Transform& offset = Transform());
	private:
		struct MeshFrame
		{
			std::vector<VertexSceneData> vertices;
			std::vector<TriangleSceneData> triangles;
			std::vector<Material> materials;
			//std::vector<SubBvhNode> bvh_nodes;
			//uint32_t bvh_root_node;
		};

		bool _bvh_needs_update;

		bool _refitting;
		std::vector<SubBvhNode> _bvh_nodes;
		uint32_t _bvh_root_node;

		unsigned _current_frame;
		std::vector<MeshFrame> _frames;
	};
}