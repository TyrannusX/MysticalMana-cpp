#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <vector>
#include <string>
#include "Vertex.h"

class AssetManager
{
	public:
		std::vector<Vertex> GetMeshVertices(const std::string& file_path);
};
#endif