#pragma once
#include "types.h"
#include "transform.h"
#include "vertices.h"
#include "imesh.h"
#include "bvh_nodes.h"
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
		const std::vector<u32>& getEmissiveTriangles() const override { return std::vector<u32>(); }// Not implemented yet

		u32 getBvhRootNode() const override { return _bvh_root_node; };

		bool isDynamic() const override { return true; };
		u32 maxNumVertices() const override;
		u32 maxNumTriangles() const override;
		u32 maxNumMaterials() const override;
		u32 maxNumBvhNodes() const override;
		void buildBvh() override;
	private:
		void addSubMesh(const aiScene* scene,
			uint mesh_index,
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
			//u32 bvh_root_node;
		};

		bool _bvh_needs_update;

		bool _refitting;
		std::vector<SubBvhNode> _bvh_nodes;
		u32 _bvh_root_node;

		uint _current_frame;
		std::vector<MeshFrame> _frames;
	};
}