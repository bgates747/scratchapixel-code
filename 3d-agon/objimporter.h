// (c) www.scratchapixel.com - 2024.
// Distributed under the terms of the CC BY-NC-ND 4.0 License.
// https://creativecommons.org/licenses/by-nc-nd/4.0/
// clang++ -Wall -Wextra -std=c++23 -o objimporter.exe objimporter.cc -O3

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <string>
#include <cassert>

#include "geometry.h"

struct ObjData {
    std::vector<Vec3f> vertices;
    std::vector<uint32_t> face_indices;
    std::vector<Vec3f> normals;
    std::vector<uint32_t> normal_indices;
    std::vector<Vec2f> uvs;
    std::vector<uint32_t> uv_indices;
};

struct FaceVertex {
	int vertex_index{-1};
	int st_coord_index{-1};
	int normal_index{-1};
};

struct FaceGroup {
	std::vector<FaceVertex> face_vertices;
	std::string name;
};

std::deque<FaceGroup> face_groups;

void ParseFaceVertex(const std::string& tuple, FaceVertex& face_vertex) {
	std::istringstream stream(tuple);
	std::string part;
	std::getline(stream, part, '/');
	face_vertex.vertex_index = std::stoi(part) - 1;
	if (std::getline(stream, part, '/') && !part.empty()) {
		face_vertex.st_coord_index = std::stoi(part) - 1;
	}
	if (std::getline(stream, part, '/') && !part.empty()) {
		face_vertex.normal_index = std::stoi(part) - 1;
	}
}

void ProcessFace(const std::vector<std::string>& tuples, std::vector<FaceVertex>& face_vertices) {
	assert(tuples.size() == 3);
	for (const auto& tuple : tuples) {
		FaceVertex face_vertex;
		ParseFaceVertex(tuple, face_vertex);
		face_vertices.push_back(face_vertex);
	}
}

ObjData ParseObj(const std::string& filename) {
    ObjData data;
    std::ifstream ifs(filename);
    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream stream(line);
        std::string type;
        stream >> type;
        if (type == "v") {
            Vec3f v;
            stream >> v.x >> v.y >> v.z;
            data.vertices.push_back(v);
        } else if (type == "vt") {
            Vec2f st;
            stream >> st.x >> st.y;
            data.uvs.push_back(st);
        } else if (type == "vn") {
            Vec3f n;
            stream >> n.x >> n.y >> n.z;
            data.normals.push_back(n);
        } else if (type == "f") {
            std::vector<std::string> face;
            std::string tuple;
            while (stream >> tuple) {
                FaceVertex fv;
                ParseFaceVertex(tuple, fv);
                data.face_indices.push_back(fv.vertex_index);
                if (fv.st_coord_index != -1) {
                    data.uv_indices.push_back(fv.st_coord_index);
                }
                if (fv.normal_index != -1) {
                    data.normal_indices.push_back(fv.normal_index);
                }
            }
        }
    }
    ifs.close();
    return data;
}
