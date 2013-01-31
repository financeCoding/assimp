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

#include "SpectreExporter.h"
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

// ------------------------------------------------------------------------------------------------
SpectreExporter :: SpectreExporter(const char* _filename, const aiScene* pScene)
: filename(_filename)
, pScene(pScene)
, endl("\n")
, indent("\t")
{
	// Make sure that all formatting happens using the standard, C locale and not the user's current locale
	const std::locale& l = std::locale("C");
	mOutput.imbue(l);

	aiNode* node = pScene->mRootNode->mChildren[1];
	//aiAnimation* anim = pScene->mAnimations[0];

	// Write the header
	mOutput << "{" << endl;

	WriteNode(pScene->mRootNode->mChildren[0]);
/*
	mOutput << indent << "\"meshes\": [" << endl;

	const unsigned int numMeshesMinusOne = pScene->mNumMeshes - 1;

	for (unsigned int i = 0; i < numMeshesMinusOne; ++i) {
		WriteMesh(pScene->mMeshes[i]);

		mOutput << "," << endl;
	}

	WriteMesh(pScene->mMeshes[numMeshesMinusOne]);

	// Write the footer
	mOutput << endl;
	mOutput << indent << "]" << endl;
*/
	mOutput << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteHeader()
{

}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteNode(const aiNode* node)
{
	const std::string scopeIndent = indent + indent;

	// Write the attributes
	// Assuming that the vertex attributes will be the same for a grouping of meshes
	WriteMeshInputLayout(pScene->mMeshes[node->mMeshes[0]]);
	mOutput << "," << endl;

	// Write the individual meshes
	unsigned int meshOffset = 0;
	unsigned int meshCountMinusOne = node->mNumMeshes - 1;

	mOutput << indent << "\"meshes\": [" << endl;

	const aiMesh* mesh;

	for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
		mesh = GetMesh(node, i);

		WriteMeshPart(mesh, &meshOffset);
		mOutput << "," << endl;
	}

	mesh = GetMesh(node, meshCountMinusOne);
	WriteMeshPart(mesh, &meshOffset);
	mOutput << endl << indent << "]," << endl;

	// Write the primitive type
	mOutput << indent << "\"primitive\": \"triangles\"," << endl;

	// Write the vertices
	mOutput << indent << "\"vertices\": [" << endl;

	for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
		mesh = GetMesh(node, i);

		WriteMeshVertices(mesh);
		mOutput << "," << endl;
	}

	mesh = GetMesh(node, meshCountMinusOne);
	WriteMeshVertices(mesh);
	mOutput << endl << indent << "]," << endl;

	// Write the indices
	mOutput << indent << "\"indices\": [" << endl;
	meshOffset = 0;

	for (unsigned int i = 0; i < meshCountMinusOne; ++i) {
		mesh = GetMesh(node, i);

		WriteIndexData(mesh, &meshOffset);
		mOutput << "," << endl;
	}

	mesh = GetMesh(node, meshCountMinusOne);
	WriteIndexData(mesh, &meshOffset);
	mOutput << endl << indent << "]" << endl;
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshPart(const aiMesh* mesh, unsigned int* offset)
{
	const std::string scopeIndent = indent + indent;

	// Start the scope
	mOutput << scopeIndent << "{" << endl;

	// Write the offset and count
	unsigned int indexCount = mesh->mNumFaces * 3;
	mOutput << scopeIndent << indent << "\"offset\": " << *offset << "," << endl;
	mOutput << scopeIndent << indent << "\"count\": " << indexCount << endl;
	
	// Increment the count
	*offset += indexCount;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshBounds(const aiMesh* mesh)
{
	const std::string scopeIndent = indent + indent + indent;

	// Start the scope
	mOutput << scopeIndent << "\"bounds\": [" << endl;

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
	mOutput << scopeIndent << indent << min.x << ", " << min.y << ", " << min.z << "," << endl;
	mOutput << scopeIndent << indent << max.x << ", " << min.y << ", " << max.z << endl;

	// End the scope
	mOutput << scopeIndent << "]";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshInputLayout(const aiMesh* mesh)
{
	const std::string scopeIndent = indent;

	// Start the scope
	mOutput << scopeIndent << "\"attributes\": [" << endl;

	const unsigned int floatSize = sizeof(float);
	unsigned int stride = GetVertexStride(mesh);
	unsigned int offset = 0;

	WriteMeshVertexAttribute("POSITION", "float", 3, stride, offset);
	offset += floatSize * 3;

	// Output normals
	if (mesh->HasNormals()) {
		mOutput << "," << endl;
		WriteMeshVertexAttribute("NORMAL", "float", 3, stride, offset);
		offset += floatSize * 3;
	}

	// Output tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		mOutput << "," << endl;
		WriteMeshVertexAttribute("TANGENT", "float", 3, stride, offset);
		offset += floatSize * 3;

		mOutput << "," << endl;
		WriteMeshVertexAttribute("BITANGENT", "float", 3, stride, offset);
		offset += floatSize * 3;
	}

	// Output all texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();
	
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		std::string name = "TEXCOORDX";
		name[8] = (char)(c + 48);
		mOutput << "," << endl;
		const unsigned int components = mesh->mNumUVComponents[c];
		WriteMeshVertexAttribute(name.c_str(), "float", components, stride, offset);
		offset += floatSize * components;
	}

	// Output all color channels
	const unsigned int numColorChannels = mesh->GetNumColorChannels();

	for (unsigned int c = 0; c < numColorChannels; ++c) {
		std::string name = "COLORX";
		name[5] = (char)(c + 48);
		mOutput << "," << endl;
		WriteMeshVertexAttribute(name.c_str(), "float", 4, stride, offset);
		offset += floatSize * 4;
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
	mOutput << scopeIndent << position.x << ", " << position.y << ", " << position.z;

	// Output normals
	if (mesh->HasNormals()) {
		aiVector3D normal = mesh->mNormals[index];
		mOutput << ", " << normal.x << ", " << normal.y << ", " << normal.z;
	}

	// Output tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		aiVector3D tangent = mesh->mTangents[index];
		mOutput << ", " << tangent.x << ", " << tangent.y << ", " << tangent.z;

		aiVector3D bitangent = mesh->mBitangents[index];
		mOutput << ", " << bitangent.x << ", " << bitangent.y << ", " << bitangent.z;
	}

	// Output all texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();
	
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		aiVector3D texCoord = mesh->mTextureCoords[c][index];

		const unsigned int uvComponents = mesh->mNumUVComponents[c];

		for (unsigned int i = 0; i < uvComponents; ++i) {
			mOutput << ", " << texCoord[i];
		}
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

	// Write out indice data
	const unsigned int numFacesMinusOne = mesh->mNumFaces - 1;
	unsigned int* faceIndices;
	unsigned int face0;
	unsigned int face1;
	unsigned int face2;

	for (unsigned i = 0; i < numFacesMinusOne; ++i) {
		faceIndices = mesh->mFaces[i].mIndices;

		face0 = faceIndices[0] + (*offset);
		face1 = faceIndices[1] + (*offset);
		face2 = faceIndices[2] + (*offset);

		mOutput << scopeIndent << face0 << ", " << face1 << ", " << face2 << "," << endl;
	}

	faceIndices = mesh->mFaces[numFacesMinusOne].mIndices;

	face0 = faceIndices[0] + (*offset);
	face1 = faceIndices[1] + (*offset);
	face2 = faceIndices[2] + (*offset);

	mOutput << scopeIndent << face0 << ", " << face1 << ", " << face2;

	*offset += mesh->mNumFaces * 3;
}

// ------------------------------------------------------------------------------------------------
unsigned int SpectreExporter ::  GetVertexStride(const aiMesh* mesh)
{
	const unsigned int floatSize = sizeof(float);

	// Position
	unsigned int stride = floatSize * 3;

	// Normals
	if (mesh->HasNormals()) {
		stride += floatSize * 3;
	}

	// Tangent/bitangent
	if (mesh->HasTangentsAndBitangents()) {
		stride += floatSize * 6;
	}

	// Texture coordinates
	const unsigned int numUVChannels = mesh->GetNumUVChannels();
	
	for (unsigned int c = 0; c < numUVChannels; ++c) {
		stride += floatSize * mesh->mNumUVComponents[c];
	}

	// Color channels
	stride += floatSize * 4 * mesh->GetNumColorChannels();

	return stride;
}

#endif
