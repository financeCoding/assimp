/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2012, assimp team
All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the 
following conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/

#include "AssimpPCH.h"

#if !defined(ASSIMP_BUILD_NO_EXPORT) && !defined(ASSIMP_BUILD_NO_SPECTRE_EXPORTER)

#define SPECTRE_PAD_VERTEX_DATA

#include "SpectreExporter.h"
#include "TinyFormatter.h"
#include "../include/assimp/version.h"

using namespace Assimp;
namespace Assimp	{

// ------------------------------------------------------------------------------------------------
// Worker function for exporting a scene to Spectre. Prototyped and registered in Exporter.cpp
void ExportSceneSpectre(const char* pFile,IOSystem* pIOSystem, const aiScene* pScene)
{
	// invoke the exporter 
	SpectreExporter exporter(pFile, pScene);

	// we're still here - export successfully completed. Write the file.
	boost::scoped_ptr<IOStream> outfile (pIOSystem->Open(pFile,"wt"));
	outfile->Write( exporter.mOutput.str().c_str(), static_cast<size_t>(exporter.mOutput.tellp()),1);
}

} // end of namespace Assimp

const unsigned int floatSize = sizeof(float);

#ifdef SPECTRE_PAD_VERTEX_DATA
const unsigned int SpectreExporter::positionSize  = 4 * floatSize;
const unsigned int SpectreExporter::normalSize    = 4 * floatSize;
const unsigned int SpectreExporter::tangentSize   = 4 * floatSize;
const unsigned int SpectreExporter::bitangentSize = 4 * floatSize;
const unsigned int SpectreExporter::colorSize     = 4 * floatSize;
#else
const unsigned int SpectreExporter::positionSize  = 3 * floatSize;
const unsigned int SpectreExporter::normalSize    = 3 * floatSize;
const unsigned int SpectreExporter::tangentSize   = 3 * floatSize;
const unsigned int SpectreExporter::bitangentSize = 3 * floatSize;
const unsigned int SpectreExporter::colorSize     = 4 * floatSize;
#endif

unsigned int min(unsigned int a, unsigned int b)
{
	return (a < b) ? a : b;
}

unsigned int min(unsigned int current, unsigned int a0, unsigned int a1, unsigned int a2)
{
	current = min(current, a0);
	current = min(current, a1);
	current = min(current, a2);

	return current;
}

unsigned int max(unsigned int a, unsigned int b)
{
	return (a > b) ? a : b;
}

unsigned int max(unsigned int current, unsigned int a0, unsigned int a1, unsigned int a2)
{
	current = max(current, a0);
	current = max(current, a1);
	current = max(current, a2);

	return current;
}

// ------------------------------------------------------------------------------------------------
SpectreExporter :: SpectreExporter(const char* _filename, const aiScene* pScene)
: filename(_filename)
, pScene(pScene)
, endl("\n")
, indent("\t")
, logger(DefaultLogger::get())
{
	// Make sure that all formatting happens using the standard, C locale and not the user's current locale
	const std::locale& l = std::locale("C");
	mOutput.imbue(l);

	// Write the header
	mOutput << "{" << endl;
	WriteHeader();

	// Write out the mesh file.
	// Currently assuming MD5
	const aiNode* rootNode = pScene->mRootNode;
	WriteNode(rootNode->mChildren[0], rootNode->mChildren[1]->mChildren[0]);

	mOutput << endl << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteHeader()
{

}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteNode(const aiNode* meshNode, const aiNode* hierarchyNode)
{
	const std::string scopeIndent = indent + indent;

	// Write the attributes
	// Assuming that the vertex attributes will be the same for a grouping of meshes
	logger->debug("Writing mesh input layout");
	WriteMeshInputLayout(pScene->mMeshes[meshNode->mMeshes[0]]);
	mOutput << "," << endl;

	// Write the individual meshes
	unsigned int meshOffset = 0;
	const unsigned int meshCountMinusOne = meshNode->mNumMeshes - 1;

	mOutput << indent << "\"meshes\": [" << endl;

	const aiMesh* mesh;

	for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
		mesh = GetMesh(meshNode, i);

		WriteMeshPart(mesh, &meshOffset);
		mOutput << "," << endl;
	}

	mesh = GetMesh(meshNode, meshCountMinusOne);
	WriteMeshPart(mesh, &meshOffset);
	mOutput << endl << indent << "]," << endl;

	// Write the primitive type
	mOutput << indent << "\"primitive\": \"triangles\"," << endl;

	// Write the vertices
	mOutput << indent << "\"vertices\": [" << endl;

	for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
		mesh = GetMesh(meshNode, i);

		WriteMeshVertices(mesh);
		mOutput << "," << endl;
	}

	mesh = GetMesh(meshNode, meshCountMinusOne);
	WriteMeshVertices(mesh);
	mOutput << endl << indent << "]";

	// Write the indices
	if (mesh->HasFaces()) {
		mOutput << "," << endl << indent << "\"indices\": [" << endl;
		meshOffset = 0;

		for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
			mesh = GetMesh(meshNode, i);

			WriteIndexData(mesh, &meshOffset);
			mOutput << "," << endl;
		}

		mesh = GetMesh(meshNode, meshCountMinusOne);
		WriteIndexData(mesh, &meshOffset);
		mOutput << endl << indent << "]";
	} else {
		logger->info("No indices specified");
		mOutput << "\"indices\": []";
	}

	// Write the bones
	if (mesh->HasBones()) {
		mOutput << "," << endl << indent << "\"bones\": [" << endl;

		WriteMeshBones(meshNode, hierarchyNode);
/*

		// Write the root node
		WriteBone(0, hierarchyNode);
		mOutput << endl << scopeIndent << "}," << endl;

		for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
			mesh = GetMesh(meshNode, i);

			WriteMeshBones(mesh, hierarchyNode);
			mOutput << "," << endl;
		}

		mesh = GetMesh(meshNode, meshCountMinusOne);
		WriteMeshBones(mesh, hierarchyNode);
*/
		mOutput << endl << indent << "]";
	} else {
		logger->info("No bones specified");
		mOutput << "\"bones\": []";
	}

	// Write the animations
	if (pScene->HasAnimations()) {
		const unsigned int animationCountMinusOne = pScene->mNumAnimations - 1;

		mOutput << "," << endl << indent << "\"animations\": [" << endl;

		for (unsigned int i = 0; i < animationCountMinusOne; ++i) {
			WriteAnimation(pScene->mAnimations[i]);
			mOutput << "," << endl;
		}

		WriteAnimation(pScene->mAnimations[animationCountMinusOne]);

		mOutput << endl << indent << "]";
	} else {
		logger->info("No animations specified");
		mOutput << "\"animations\": []";
	}
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshPart(const aiMesh* mesh, unsigned int* offset)
{
	const std::string scopeIndent = indent + indent;

	// Start the scope
	mOutput << scopeIndent << "{" << endl;

	// Write the offset and count
	// \todo Compute the index size
	const unsigned int indexCount = mesh->mNumFaces * 3;
	mOutput << scopeIndent << indent << "\"offset\": " << (*offset) * 2 << "," << endl;
	mOutput << scopeIndent << indent << "\"count\": " << indexCount << "," << endl;

	// Write the mesh bounds
	WriteMeshBounds(mesh);
	
	// Increment the count
	*offset += indexCount;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshBounds(const aiMesh* mesh)
{
	const std::string scopeIndent = indent + indent + indent;

	// Determine the bounds
	aiVector3D min(std::numeric_limits<float>::max());
	aiVector3D max(std::numeric_limits<float>::min());

	unsigned int vertexCount = mesh->mNumVertices;

	for (unsigned int i = 0; i < vertexCount; ++i) {
		aiVector3D position = mesh->mVertices[i];

		min.x = (position.x < min.x) ? position.x : min.x;
		min.y = (position.y < min.y) ? position.y : min.y;
		min.z = (position.z < min.z) ? position.z : min.z;

		max.x = (position.x > max.x) ? position.x : max.x;
		max.y = (position.y > max.y) ? position.y : max.y;
		max.z = (position.z > max.z) ? position.z : max.z;
	}

	// Write out the bounds
	mOutput << scopeIndent << "\"aabbMin\": [" << min.x << ", " << min.y << ", " << min.z << "]," << endl;
	mOutput << scopeIndent << "\"aabbMax\": [" << max.x << ", " << min.y << ", " << max.z << "]"  << endl;
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshInputLayout(const aiMesh* mesh)
{
	const std::string scopeIndent = indent;

	// Start the scope
	mOutput << scopeIndent << "\"attributes\": [" << endl;

	const unsigned int stride = GetVertexStride(mesh);
	unsigned int offset = 0;

	WriteMeshVertexAttribute("POSITION", "float", 3, stride, offset);
	offset += positionSize;

	// Output normals
	if (mesh->HasNormals()) {
		mOutput << "," << endl;
		WriteMeshVertexAttribute("NORMAL", "float", 3, stride, offset);
		offset += normalSize;
	}

	// Output tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		mOutput << "," << endl;
		WriteMeshVertexAttribute("TANGENT", "float", 3, stride, offset);
		offset += tangentSize;

		mOutput << "," << endl;
		WriteMeshVertexAttribute("BITANGENT", "float", 3, stride, offset);
		offset += bitangentSize;
	}

	// Output all texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();
	
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		std::string name = "TEXCOORDX";
		name[8] = (char)(c + 48);
		mOutput << "," << endl;
		const unsigned int components = mesh->mNumUVComponents[c];
		WriteMeshVertexAttribute(name.c_str(), "float", components, stride, offset);
#ifdef SPECTRE_PAD_VERTEX_DATA
		offset += floatSize * 4;
#else
		offset += floatSize * components;
#endif
	}

	// Output all color channels
	const unsigned int numColorChannels = mesh->GetNumColorChannels();

	for (unsigned int c = 0; c < numColorChannels; ++c) {
		std::string name = "COLORX";
		name[5] = (char)(c + 48);
		mOutput << "," << endl;
		WriteMeshVertexAttribute(name.c_str(), "float", 4, stride, offset);
		offset += colorSize;
	}

	// End the scope
	mOutput << endl << scopeIndent << "]";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshVertexAttribute(const char* name, const char* type, unsigned int components, unsigned int stride, unsigned int offset)
{
	const std::string scopeIndent = indent + indent;
	const std::string attribIndent = scopeIndent + indent;

	// Start the scope
	mOutput << scopeIndent << "{" << endl;

	// Write out the vertex attributes
	mOutput << attribIndent << "\"name\": \"" << name << "\"," << endl;
	mOutput << attribIndent << "\"offset\": " << offset << "," << endl;
	mOutput << attribIndent << "\"stride\": " << stride << "," << endl;
	mOutput << attribIndent << "\"format\": \"" << type << components << "\"" << endl;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshVertices(const aiMesh* mesh)
{
	// Write out vertices
	const unsigned int numVerticesMinusOne = mesh->mNumVertices - 1;

	for (unsigned int i = 0; i < numVerticesMinusOne; ++i) {
		WriteVertexData(mesh, i);
		mOutput << "," << endl;
	}

	WriteVertexData(mesh, numVerticesMinusOne);
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteVertexData(const aiMesh* mesh, unsigned int index)
{
	const std::string scopeIndent = indent + indent;

	// Output positions
	aiVector3D position = mesh->mVertices[index];
#ifdef SPECTRE_PAD_VERTEX_DATA
	mOutput << scopeIndent << position.x << ", " << position.y << ", " << position.z << ", 1.0";
#else
	mOutput << scopeIndent << position.x << ", " << position.y << ", " << position.z;
#endif
	// Output normals
	if (mesh->HasNormals()) {
		aiVector3D normal = mesh->mNormals[index];
#ifdef SPECTRE_PAD_VERTEX_DATA
		mOutput << ", " << normal.x << ", " << normal.y << ", " << normal.z << ", 0.0";
#else
		mOutput << ", " << normal.x << ", " << normal.y << ", " << normal.z;
#endif
	}

	// Output tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		aiVector3D tangent = mesh->mTangents[index];

#ifdef SPECTRE_PAD_VERTEX_DATA
		mOutput << ", " << tangent.x << ", " << tangent.y << ", " << tangent.z << ", 0.0";
#else
		mOutput << ", " << tangent.x << ", " << tangent.y << ", " << tangent.z;
#endif
		aiVector3D bitangent = mesh->mBitangents[index];

#ifdef SPECTRE_PAD_VERTEX_DATA
		mOutput << ", " << bitangent.x << ", " << bitangent.y << ", " << bitangent.z << ", 0.0";
#else
		mOutput << ", " << bitangent.x << ", " << bitangent.y << ", " << bitangent.z;
#endif
	}

	// Output all texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();
	
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		aiVector3D texCoord = mesh->mTextureCoords[c][index];

		const unsigned int uvComponents = mesh->mNumUVComponents[c];

		for (unsigned int i = 0; i < uvComponents; ++i) {
			mOutput << ", " << texCoord[i];
		}

#ifdef SPECTRE_PAD_VERTEX_DATA
		for (unsigned int i = uvComponents; i < 4; ++i) {
			mOutput << ", 0.0";
		}
#endif
	}

	// Output all color channels
	const unsigned int numColorChannels = mesh->GetNumColorChannels();

	for (unsigned int c = 0; c < numColorChannels; ++c) {
		aiColor4D color = mesh->mColors[c][index];

		mOutput << ", " << color.r << ", " << color.g << ", " << color.b << ", " << color.a;
	}
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteIndexData(const aiMesh* mesh, unsigned int* offset)
{
	const std::string scopeIndent = indent + indent;

	std::cout << "New mesh: " << mesh->mName.C_Str() << endl;
	std::cout << "Vertex count: " << mesh->mNumVertices << endl;
	std::cout << "Offset: " << *offset << endl;

	// Write out indice data
	const unsigned int numFacesMinusOne = mesh->mNumFaces - 1;
	unsigned int* faceIndices;
	unsigned int face0;
	unsigned int face1;
	unsigned int face2;

	unsigned int minFace = 99999999;
	unsigned int maxFace = 0;

	for (unsigned i = 0; i < numFacesMinusOne; ++i) {
		faceIndices = mesh->mFaces[i].mIndices;

		face0 = faceIndices[0] + (*offset);
		face1 = faceIndices[1] + (*offset);
		face2 = faceIndices[2] + (*offset);

		minFace = min(minFace, face0, face1, face2);
		maxFace = max(maxFace, face0, face1, face2);

		mOutput << scopeIndent << face0 << ", " << face1 << ", " << face2 << "," << endl;
	}

	faceIndices = mesh->mFaces[numFacesMinusOne].mIndices;

	face0 = faceIndices[0] + (*offset);
	face1 = faceIndices[1] + (*offset);
	face2 = faceIndices[2] + (*offset);

	minFace = min(minFace, face0, face1, face2);
	maxFace = max(maxFace, face0, face1, face2);

	std::cout << "Min index: " << minFace << ", " << minFace - (*offset) << endl;
	std::cout << "Max index: " << maxFace << ", " << maxFace - (*offset) << endl << endl;

	mOutput << scopeIndent << face0 << ", " << face1 << ", " << face2;

	*offset += mesh->mNumVertices;
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshBones(const aiNode* meshNode, const aiNode* boneNode, int depth)
{
	if (!boneNode) {
		return;
	}

	const std::string scopeIndent = indent + indent;
	const std::string boneIndent = scopeIndent + indent;

	// Begin the scope
	if (depth != 0) {
		mOutput << "," << endl;
	}

	mOutput << scopeIndent << "{" << endl;

	// Write the name
	mOutput << boneIndent << "\"name\": \"" << boneNode->mName.C_Str() << "\"," << endl;

	// Output the transform
	WriteTransform("transform", boneNode->mTransformation);
	mOutput << "," << endl;

	// Write children
	const unsigned int numChildren = boneNode->mNumChildren;
	logger->debug(boost::str(boost::format("Writing bone \"%s\" with %d children.") % boneNode->mName.C_Str() % numChildren));

	mOutput << boneIndent << "\"children\": [";

	if (numChildren != 0) {
		const unsigned int numChildrenMinusOne = numChildren - 1;
		const aiNode* childBone;
	
		for (unsigned int i = 0; i < numChildrenMinusOne; ++i) {
			childBone = boneNode->mChildren[i];

			logger->debug(boost::str(boost::format("Child bone: %s") % childBone->mName.C_Str()));
			mOutput << "\"" << childBone->mName.C_Str() << "\", ";
		}

		childBone = boneNode->mChildren[numChildrenMinusOne];

		logger->debug(boost::str(boost::format("Child bone: %s") % childBone->mName.C_Str()));
		mOutput << "\"" << childBone->mName.C_Str() << "\"";
	}

	mOutput << "]," << endl;

	// Get bone information
	const unsigned int numMeshes = meshNode->mNumMeshes;
	unsigned int offset = 0;

	std::vector<unsigned int> vertices;
	std::vector<float> weights;
	const aiBone* bone;

	for (unsigned int i = 0; i < numMeshes; ++i) {
		const aiMesh* mesh = GetMesh(meshNode, i);

		// Iterate over bones
		const unsigned int numBones = mesh->mNumBones;

		for (unsigned int j = 0; j < numBones; ++j) {
			bone = mesh->mBones[j];

			if (bone->mName == boneNode->mName) {
				logger->debug(boost::str(boost::format("Mesh %s at index %d uses bone %s") % mesh->mName.C_Str() % i % boneNode->mName.C_Str()));

				// Output all the vertices
				const unsigned int numWeights = bone->mNumWeights;

				for (unsigned int b = 0; b < numWeights; ++b) {
					const aiVertexWeight vertexWeight = bone->mWeights[b];

					vertices.push_back(vertexWeight.mVertexId + offset);
					weights.push_back(vertexWeight.mWeight);
				}

				break;
			}
		}

		// Increment the offset
		offset += mesh->mNumVertices;
	}

	// Output the offset matrix
	// Check for the root node
	if (depth == 0) {
		// Invert the transform for the root node
		aiMatrix4x4 inverse(boneNode->mTransformation);
		inverse.Inverse();

		WriteTransform("offsetTransform", inverse);
	} else {
		WriteTransform("offsetTransform", bone->mOffsetMatrix);
	}

	mOutput << "," << endl;

	const unsigned int numWeights = vertices.size();
	const unsigned int numWeightsMinusOne = numWeights - 1;

	// Write vertices
	mOutput << boneIndent << "\"vertices\": [";

	if (numWeights > 0) {

		for (unsigned int i = 0; i < numWeightsMinusOne; ++i) {
			mOutput << vertices[i] << ", ";
		}

		mOutput << vertices[numWeightsMinusOne];
	}
	mOutput << "]," << endl;

	// Write weights
	mOutput << boneIndent << "\"weights\": [";

	if (numWeights > 0) {

		for (unsigned int i = 0; i < numWeightsMinusOne; ++i) {
			mOutput << weights[i] << ", ";
		}

		mOutput << weights[numWeightsMinusOne];
	}
	mOutput << "]" << endl;

	// End the scope
	mOutput << scopeIndent << "}";

	// Write the rest of the hierarchy
	const unsigned int newDepth = depth + 1;

	for (unsigned int i = 0; i < numChildren; ++i) {
		WriteMeshBones(meshNode, boneNode->mChildren[i], newDepth);
	}
}

/*
// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshBones(const aiMesh* mesh, const aiNode* hierarchyNode)
{
	const unsigned int boneCountMinusOne = mesh->mNumBones - 1;

	for (unsigned int i = 0; i < boneCountMinusOne; ++i) {
		WriteBone(mesh->mBones[i], hierarchyNode);
		mOutput << "," << endl;
	}

	WriteBone(mesh->mBones[boneCountMinusOne], hierarchyNode);
}
*/
// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteBone(const aiBone* bone, const aiNode* hierarchyNode)
{
	const std::string scopeIndent = indent + indent;
	const std::string boneIndent = scopeIndent + indent;

	// Begin the scope
	mOutput << scopeIndent << "{" << endl;

	// Find the node
	// Check for the root node
	// \todo Does the origin node always not affect any vertices? Seems to on MD5
	const aiNode* boneNode = (bone)
		? FindBoneInHierarchy(bone->mName, hierarchyNode)
		: hierarchyNode;

	// Output the bone name
	mOutput << boneIndent << "\"name\": \"" << boneNode->mName.C_Str() << "\"," << endl;

	// Output the transform
	WriteTransform("transform", boneNode->mTransformation);
	mOutput << "," << endl;

	// Output the offset matrix
	// Check for the root node
	if (boneNode == hierarchyNode) {
		// Invert the transform for the root node
		aiMatrix4x4 inverse(boneNode->mTransformation);
		inverse.Inverse();

		WriteTransform("offsetTransform", inverse);
	} else {
		WriteTransform("offsetTransform", bone->mOffsetMatrix);
	}

	// Don't output if there are no affected vertices
	if (!bone) {
		return;
	}

	// Output the vertices
	const unsigned int numVertexWeightsMinusOne = bone->mNumWeights - 1;

	if (numVertexWeightsMinusOne != -1) {
		mOutput << "," << endl << boneIndent << "\"vertices\": [";

		for (unsigned int i = 0; i < numVertexWeightsMinusOne; ++i) {
			mOutput << bone->mWeights[i].mVertexId << ", ";
		}

		mOutput << bone->mWeights[numVertexWeightsMinusOne].mVertexId << "],";

		// Output the weights
		mOutput << endl << boneIndent << "\"weights\": [";

		for (unsigned int i = 0; i < numVertexWeightsMinusOne; ++i) {
			mOutput << bone->mWeights[i].mWeight << ", ";
		}

		mOutput << bone->mWeights[numVertexWeightsMinusOne].mWeight << "]";
	}

	// Output the children
	const unsigned int numChildrenMinusOne = boneNode->mNumChildren - 1;

	if (numChildrenMinusOne != -1) {
		mOutput << "," << endl << boneIndent << "\"children\": [";

		for (unsigned int i = 0; i < numChildrenMinusOne; ++i) {
			mOutput << "\"" << boneNode->mChildren[i]->mName.C_Str() << "\", ";
		}

		mOutput << "\"" << boneNode->mChildren[numChildrenMinusOne]->mName.C_Str() << "\"]";
	}

	// End the scope
	mOutput << endl << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteTransform(const char* name, const aiMatrix4x4& transform)
{
	const std::string scopeIndent = indent + indent + indent;
	const std::string rowIndent = scopeIndent + indent;

	mOutput << scopeIndent << "\"" << name << "\": [" << endl;
	mOutput << rowIndent << transform.a1 << ", " << transform.a2 << ", " << transform.a3 << ", " << transform.a4 << "," << endl;
	mOutput << rowIndent << transform.b1 << ", " << transform.b2 << ", " << transform.b3 << ", " << transform.b4 << "," << endl;
	mOutput << rowIndent << transform.c1 << ", " << transform.c2 << ", " << transform.c3 << ", " << transform.c4 << "," << endl;
	mOutput << rowIndent << transform.d1 << ", " << transform.d2 << ", " << transform.d3 << ", " << transform.d4 << endl;
	mOutput << scopeIndent << "]";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter ::  WriteAnimation(const aiAnimation* animation)
{
	const std::string scopeIndent = indent + indent;
	const std::string animationIndent = scopeIndent + indent;

	// Begin the scope
	mOutput << scopeIndent << "{" << endl;

	// Output the animation name
	mOutput << animationIndent << "\"name\": \"" << animation->mName.C_Str() << "\"," << endl;

	// Output the ticks per second
	mOutput << animationIndent << "\"ticksPerSecond\": " << animation->mTicksPerSecond << "," << endl;

	// Output the duration
	mOutput << animationIndent << "\"duration\": " << animation->mDuration << "," << endl;

	// Output the channels
	const unsigned int numChannelsMinusOne = animation->mNumChannels - 1;

	mOutput << animationIndent << "\"boneAnimations\": [" << endl;

	for (unsigned int i = 0; i < numChannelsMinusOne; ++i) {
		WriteAnimationChannel(animation->mChannels[i]);
		mOutput << "," << endl;
	}

	WriteAnimationChannel(animation->mChannels[numChannelsMinusOne]);
	mOutput << endl << animationIndent << "]";

	// End the scope
	mOutput << endl << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteAnimationChannel(const aiNodeAnim* channel)
{
	const std::string scopeIndent = indent + indent + indent + indent;
	const std::string channelIndent = scopeIndent + indent;

	// Begin the scope
	mOutput << scopeIndent << "{" << endl;

	// Write the channel name
	mOutput << channelIndent << "\"name\": \"" << channel->mNodeName.C_Str() << "\"," << endl;

	// Write the position keys
	const unsigned int numPositionKeysMinusOne = channel->mNumPositionKeys - 1;

	mOutput << channelIndent << "\"positions\": [" << endl;

	for (unsigned int i = 0; i < numPositionKeysMinusOne; ++i) {
		WriteVectorKey(channel->mPositionKeys[i]);
		mOutput << "," << endl;
	}

	WriteVectorKey(channel->mPositionKeys[numPositionKeysMinusOne]);
	mOutput << endl << channelIndent << "]," << endl;

	// Write the rotation keys
	const unsigned int numRotationKeysMinusOne = channel->mNumRotationKeys - 1;

	mOutput << channelIndent << "\"rotations\": [" << endl;

	for (unsigned int i = 0; i < numRotationKeysMinusOne; ++i) {
		WriteQuaternionKey(channel->mRotationKeys[i]);
		mOutput << "," << endl;
	}

	WriteQuaternionKey(channel->mRotationKeys[numRotationKeysMinusOne]);
	mOutput << endl << channelIndent << "]," << endl;

	// Write the scale keys
	const unsigned int numScalingKeysMinusOne = channel->mNumScalingKeys - 1;

	mOutput << channelIndent << "\"scales\": [" << endl;

	for (unsigned int i = 0; i < numScalingKeysMinusOne; ++i) {
		WriteVectorKey(channel->mScalingKeys[i]);
		mOutput << "," << endl;
	}

	WriteVectorKey(channel->mScalingKeys[numScalingKeysMinusOne]);
	mOutput << endl << channelIndent << "]";

	// End the scope
	mOutput << endl << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter ::  WriteVectorKey(const aiVectorKey& key)
{
	const std::string scopeIndent = indent + indent + indent + indent + indent + indent;
	const std::string keyIndent = scopeIndent + indent;

	// Begin the scope
	mOutput << scopeIndent << "{" << endl;
	
	// Write the time
	mOutput << keyIndent << "\"time\": " << key.mTime << "," << endl;
	mOutput << keyIndent << "\"value\": [ " << key.mValue.x << ", " << key.mValue.y << ", " << key.mValue.z << "]" << endl;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteQuaternionKey(const aiQuatKey& key)
{
	const std::string scopeIndent = indent + indent + indent + indent + indent + indent;
	const std::string keyIndent = scopeIndent + indent;

	// Begin the scope
	mOutput << scopeIndent << "{" << endl;
	
	// Write the time
	mOutput << keyIndent << "\"time\": " << key.mTime << "," << endl;
	mOutput << keyIndent << "\"value\": [ " << key.mValue.x << ", " << key.mValue.y << ", " << key.mValue.z << ", " << key.mValue.w << "]" << endl;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
unsigned int SpectreExporter :: GetVertexStride(const aiMesh* mesh)
{
	// Position
	unsigned int stride = positionSize;

	// Normals
	if (mesh->HasNormals()) {
		stride += normalSize;
	}

	// Tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		stride += tangentSize + bitangentSize;
	}

	// Texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();

#ifdef SPECTRE_PAD_VERTEX_DATA
	stride += floatSize * 4 * numUVChannels;
#else
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		stride += floatSize * mesh->mNumUVComponents[c];
	}
#endif

	// Color channels
	stride += colorSize * mesh->GetNumColorChannels();

	return stride;
}

// ------------------------------------------------------------------------------------------------
const aiNode* SpectreExporter :: FindBoneInHierarchy(const aiString& name, const aiNode* node)
{
	if (name == node->mName) {
		return node;
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		const aiNode* search = FindBoneInHierarchy(name, node->mChildren[i]);

		if (search) {
			return search;
		}
	}

	return 0;
}

#endif
