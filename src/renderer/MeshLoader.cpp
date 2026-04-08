#include "MeshLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdexcept>
#include <iostream>

Mesh MeshLoader::load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || !scene->mRootNode)
        throw std::runtime_error("Failed to load mesh: " + path);

    Mesh mesh;
    aiMesh* aimesh = scene->mMeshes[0];

    for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
        Vertex v{};
        v.position = { aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z };
        v.color    = {
            (v.position.x + 1.0f) * 0.5f,
            (v.position.y + 1.0f) * 0.5f,
            (v.position.z + 1.0f) * 0.5f
        };

        if (aimesh->HasNormals())
            v.normal = { aimesh->mNormals[i].x, aimesh->mNormals[i].y, aimesh->mNormals[i].z };
        else
            v.normal = { 0.0f, 1.0f, 0.0f };

        if (aimesh->mTextureCoords[0])
            v.uv = { aimesh->mTextureCoords[0][i].x, aimesh->mTextureCoords[0][i].y };
        else
            v.uv = { 0.0f, 0.0f };

        if (aimesh->HasTangentsAndBitangents()) {
            v.tangent = { aimesh->mTangents[i].x, aimesh->mTangents[i].y, aimesh->mTangents[i].z };
            v.bitangent = { aimesh->mBitangents[i].x, aimesh->mBitangents[i].y, aimesh->mBitangents[i].z };
        } else {
            v.tangent = { 1.0f, 0.0f, 0.0f };
            v.bitangent = { 0.0f, 1.0f, 0.0f };
        }

        mesh.vertices.push_back(v);
        if (aimesh->mTextureCoords[0]) {
            v.uv = {
                aimesh->mTextureCoords[0][i].x,
                aimesh->mTextureCoords[0][i].y
            };
        } else {
            v.uv = {0.0f, 0.0f};
        }
    }

    for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
        aiFace& face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            mesh.indices.push_back(face.mIndices[j]);
    }

    std::cout << "Mesh loaded: " << mesh.vertices.size()
              << " vertices, " << mesh.indices.size() << " indices" << std::endl;
    return mesh;
    
}