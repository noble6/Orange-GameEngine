#include "engine/graphics/MeshLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cmath>

using json = nlohmann::json;

void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) noexcept {
    std::vector<Vec3> tan1(vertices.size(), Vec3{0.0f, 0.0f, 0.0f});
    std::vector<Vec3> tan2(vertices.size(), Vec3{0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i1 = indices[i];
        uint32_t i2 = indices[i+1];
        uint32_t i3 = indices[i+2];

        const Vec3& v1 = vertices[i1].position;
        const Vec3& v2 = vertices[i2].position;
        const Vec3& v3 = vertices[i3].position;

        const Vec2& w1 = vertices[i1].texcoord;
        const Vec2& w2 = vertices[i2].texcoord;
        const Vec2& w3 = vertices[i3].texcoord;

        float x1 = v2.x - v1.x;
        float x2 = v3.x - v1.x;
        float y1 = v2.y - v1.y;
        float y2 = v3.y - v1.y;
        float z1 = v2.z - v1.z;
        float z2 = v3.z - v1.z;

        float s1 = w2.x - w1.x;
        float s2 = w3.x - w1.x;
        float t1 = w2.y - w1.y;
        float t2 = w3.y - w1.y;

        float r = 1.0f / (s1 * t2 - s2 * t1);
        if (std::isinf(r) || std::isnan(r)) r = 1.0f;

        Vec3 sdir{(t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r};
        Vec3 tdir{(s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r};

        tan1[i1] = tan1[i1] + sdir;
        tan1[i2] = tan1[i2] + sdir;
        tan1[i3] = tan1[i3] + sdir;

        tan2[i1] = tan2[i1] + tdir;
        tan2[i2] = tan2[i2] + tdir;
        tan2[i3] = tan2[i3] + tdir;
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        const Vec3& n = vertices[i].normal;
        const Vec3& t = tan1[i];

        Vec3 tangent = normalizedOrZero(t - n * dot(n, t));
        
        // Calculate handedness
        Vec3 crossNT = Vec3{n.y*t.z - n.z*t.y, n.z*t.x - n.x*t.z, n.x*t.y - n.y*t.x};
        float w = (dot(crossNT, tan2[i]) < 0.0f) ? -1.0f : 1.0f;

        vertices[i].tangent = Vec4{tangent.x, tangent.y, tangent.z, w};
    }
}

MeshData loadMeshGLTF(const char* path) noexcept {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return MeshData{};

        json gltf;
        file >> gltf;

        if (!gltf.contains("buffers") || gltf["buffers"].empty()) return MeshData{};
        std::string uri = gltf["buffers"][0].value("uri", "");
        if (uri.empty()) return MeshData{};

        std::string dir = std::string(path);
        size_t slashPos = dir.find_last_of("/\\");
        dir = (slashPos != std::string::npos) ? dir.substr(0, slashPos + 1) : "";
        std::string binPath = dir + uri;

        std::ifstream binFile(binPath, std::ios::binary);
        if (!binFile.is_open()) return MeshData{};
        std::vector<uint8_t> binData((std::istreambuf_iterator<char>(binFile)), std::istreambuf_iterator<char>());

        MeshData outMesh;
        std::unordered_map<Vertex, uint32_t, VertexHasher, VertexEqual> uniqueVertices;

        if (!gltf.contains("meshes")) return outMesh;

        auto getAccessorData = [&](int accessorIdx, uint8_t*& outData, uint32_t& outCount, uint32_t& outStride, int& outComponentType) {
            if (accessorIdx < 0) return false;
            auto& accessor = gltf["accessors"][accessorIdx];
            int bufferViewIdx = accessor.value("bufferView", -1);
            if (bufferViewIdx < 0) return false;
            
            auto& bufferView = gltf["bufferViews"][bufferViewIdx];
            uint32_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
            outCount = accessor.value("count", 0);
            outStride = bufferView.value("byteStride", 0);
            outComponentType = accessor.value("componentType", 0);
            outData = binData.data() + byteOffset;
            return true;
        };

        for (auto& mesh : gltf["meshes"]) {
            if (!mesh.contains("primitives")) continue;
            for (auto& prim : mesh["primitives"]) {
                int mode = prim.value("mode", 4);
                if (mode != 4) {
                    std::cerr << "Warning: Skipping GLTF primitive with mode != 4 (TRIANGLES)\n";
                    continue;
                }

                auto attributes = prim["attributes"];
                int posIdx = attributes.value("POSITION", -1);
                int normIdx = attributes.value("NORMAL", -1);
                int uvIdx = attributes.value("TEXCOORD_0", -1);
                int tanIdx = attributes.value("TANGENT", -1);
                int indIdx = prim.value("indices", -1);

                if (posIdx < 0 || normIdx < 0 || uvIdx < 0 || indIdx < 0) continue;

                uint8_t *posData = nullptr, *normData = nullptr, *uvData = nullptr, *tanData = nullptr, *indData = nullptr;
                uint32_t vCount = 0, iCount = 0, posStride = 0, normStride = 0, uvStride = 0, tanStride = 0, indStride = 0;
                int posComp = 0, normComp = 0, uvComp = 0, tanComp = 0, indComp = 0;

                if (!getAccessorData(posIdx, posData, vCount, posStride, posComp)) continue;
                if (!getAccessorData(normIdx, normData, vCount, normStride, normComp)) continue;
                if (!getAccessorData(uvIdx, uvData, vCount, uvStride, uvComp)) continue;
                if (!getAccessorData(indIdx, indData, iCount, indStride, indComp)) continue;
                bool hasTangent = getAccessorData(tanIdx, tanData, vCount, tanStride, tanComp);

                if (posStride == 0) posStride = 12;
                if (normStride == 0) normStride = 12;
                if (uvStride == 0) uvStride = 8;
                if (hasTangent && tanStride == 0) tanStride = 16;
                uint32_t indexStride = (indComp == 5123) ? 2 : 4;

                std::vector<Vertex> tempVertices(vCount);
                for (uint32_t i = 0; i < vCount; ++i) {
                    Vertex v{};
                    memcpy(&v.position, posData + i * posStride, 12);
                    memcpy(&v.normal, normData + i * normStride, 12);
                    memcpy(&v.texcoord, uvData + i * uvStride, 8);
                    if (hasTangent) memcpy(&v.tangent, tanData + i * tanStride, 16);
                    tempVertices[i] = v;
                }

                std::vector<uint32_t> tempIndices(iCount);
                for (uint32_t i = 0; i < iCount; ++i) {
                    if (indComp == 5123) {
                        uint16_t idx;
                        memcpy(&idx, indData + i * indexStride, 2);
                        tempIndices[i] = idx;
                    } else if (indComp == 5125) {
                        uint32_t idx;
                        memcpy(&idx, indData + i * indexStride, 4);
                        tempIndices[i] = idx;
                    }
                }

                if (!hasTangent) computeTangents(tempVertices, tempIndices);

                for (uint32_t idx : tempIndices) {
                    if (idx >= tempVertices.size()) continue;
                    const Vertex& v = tempVertices[idx];
                    if (uniqueVertices.count(v) == 0) {
                        uniqueVertices[v] = static_cast<uint32_t>(outMesh.vertices.size());
                        outMesh.vertices.push_back(v);
                    }
                    outMesh.indices.push_back(uniqueVertices[v]);
                }
            }
        }
        return outMesh;
    } catch (...) {
        return MeshData{};
    }
}

// Dummy implementation to avoid linking errors if someone calls it directly
MeshData loadMeshOBJ(const char* path) noexcept {
    (void)path;
    return MeshData{};
}
