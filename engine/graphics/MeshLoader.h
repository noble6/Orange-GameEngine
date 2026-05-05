#pragma once
#include "engine/graphics/AssetManager.h"

MeshData loadMeshGLTF(const char* path) noexcept;
MeshData loadMeshOBJ(const char* path) noexcept;
void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) noexcept;
