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
// Worker function for exporting a scene to PLY. Prototyped and registered in Exporter.cpp
void ExportSceneSpectre(const char* pFile,IOSystem* pIOSystem, const aiScene* pScene)
{
	// invoke the exporter 
	SpectreExporter exporter(pFile, pScene);

	// we're still here - export successfully completed. Write the file.
	boost::scoped_ptr<IOStream> outfile (pIOSystem->Open(pFile,"wt"));
	outfile->Write( exporter.mOutput.str().c_str(), static_cast<size_t>(exporter.mOutput.tellp()),1);
}

} // end of namespace Assimp

#define PLY_EXPORT_HAS_NORMALS 0x1
#define PLY_EXPORT_HAS_TANGENTS_BITANGENTS 0x2
#define PLY_EXPORT_HAS_TEXCOORDS 0x4
#define PLY_EXPORT_HAS_COLORS (PLY_EXPORT_HAS_TEXCOORDS << AI_MAX_NUMBER_OF_TEXTURECOORDS)

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

	// Write the header
	mOutput << "{" << endl;
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
	mOutput << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMesh(const aiMesh* m)
{
	const std::string scopeIndent = indent + indent;

	// Start the scope
	mOutput << scopeIndent << "{" << endl;

	// Write out vertices
	WriteMeshVertices(m);
	mOutput << "," << endl;

	// Write out indices
	WriteMeshIndices(m);
	mOutput << endl;

	// End the scope
	mOutput << scopeIndent << "}";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteMeshVertices(const aiMesh* mesh)
{
	const std::string scopeIndent = indent + indent + indent;

	// Start the scope
	mOutput << scopeIndent << "\"vertices\": [" << endl;

	// Write out vertices
	const unsigned int numVerticesMinusOne = mesh->mNumVertices - 1;

	for (unsigned int i = 0; i < numVerticesMinusOne; ++i) {
		WriteVertexData(mesh, i);
		mOutput << "," << endl;
	}

	WriteVertexData(mesh, numVerticesMinusOne);
	mOutput << endl;

	// End the scope
	mOutput << scopeIndent << "]";
}

// ------------------------------------------------------------------------------------------------
void SpectreExporter :: WriteVertexData(const aiMesh* mesh, unsigned int index)
{
	const std::string scopeIndent = indent + indent + indent + indent;

	// Output positions
	aiVector3D position = mesh->mVertices[index];
	mOutput << scopeIndent << position.x << ", " << position.y << ", " << position.z;

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
void SpectreExporter :: WriteMeshIndices(const aiMesh* mesh)
{
	const std::string scopeIndent = indent + indent + indent;

	// Start the scope
	mOutput << scopeIndent << "\"indices\": [" << endl;

	// Write out indice data
	const std::string faceIndent = scopeIndent + indent;
	const unsigned int numFacesMinusOne = mesh->mNumFaces - 1;
	unsigned int* faceIndices;

	for (unsigned i = 0; i < numFacesMinusOne; ++i) {
		faceIndices = mesh->mFaces[i].mIndices;

		mOutput << faceIndent << faceIndices[0] << ", " << faceIndices[1] << ", " << faceIndices[2] << "," << endl;
	}

	faceIndices = mesh->mFaces[numFacesMinusOne].mIndices;
	mOutput << faceIndent << faceIndices[0] << ", " << faceIndices[1] << ", " << faceIndices[2] << endl;

	// End the scope
	mOutput << scopeIndent << "]";
}

#endif
