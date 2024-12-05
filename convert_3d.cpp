#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <fstream>
#include <sstream>

// Define structures for vertices, tetrahedra, and hexahedra
struct Vertex {
    double x, y, z;
};

struct Tetrahedron {
    int v1, v2, v3, v4; // Indices of vertices
};

struct Hexahedron {
    int v1, v2, v3, v4, v5, v6, v7, v8; // Indices of vertices
};

// Custom hash function for std::pair
struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& pair) const {
        return std::hash<T1>()(pair.first) ^ (std::hash<T2>()(pair.second) << 1);
    }
};

// Utility to create a face key
std::tuple<int, int, int> makeFaceKey(int v1, int v2, int v3) {
    std::vector<int> vertices = {v1, v2, v3};
    std::sort(vertices.begin(), vertices.end());
    return {vertices[0], vertices[1], vertices[2]};
}

// Function to convert tetrahedra to hexahedra
std::vector<Hexahedron> convertToHexahedra(const std::vector<Vertex>& vertices, const std::vector<Tetrahedron>& tetrahedra) {
    // Map to store adjacent tetrahedra for each face
    std::unordered_map<std::tuple<int, int, int>, std::vector<int>, PairHash> faceToTetrahedra;

    // Fill face-to-tetrahedra map
    for (size_t i = 0; i < tetrahedra.size(); ++i) {
        const Tetrahedron& tet = tetrahedra[i];
        auto faces = {
            makeFaceKey(tet.v1, tet.v2, tet.v3),
            makeFaceKey(tet.v1, tet.v2, tet.v4),
            makeFaceKey(tet.v1, tet.v3, tet.v4),
            makeFaceKey(tet.v2, tet.v3, tet.v4)
        };
        for (const auto& face : faces) {
            faceToTetrahedra[face].push_back(i);
        }
    }

    // Set to track used tetrahedra
    std::set<int> usedTetrahedra;
    std::vector<Hexahedron> hexahedra;

    // Iterate over tetrahedra and form hexahedra
    for (size_t i = 0; i < tetrahedra.size(); ++i) {
        if (usedTetrahedra.count(i)) continue;

        const Tetrahedron& tet1 = tetrahedra[i];
        for (const auto& face : {
            makeFaceKey(tet1.v1, tet1.v2, tet1.v3),
            makeFaceKey(tet1.v1, tet1.v2, tet1.v4),
            makeFaceKey(tet1.v1, tet1.v3, tet1.v4),
            makeFaceKey(tet1.v2, tet1.v3, tet1.v4)
        }) {
            const auto& adjacentTetrahedra = faceToTetrahedra[face];
            if (adjacentTetrahedra.size() == 2) { // Only consider faces shared by exactly two tetrahedra
                int adjIndex = adjacentTetrahedra[0] == i ? adjacentTetrahedra[1] : adjacentTetrahedra[0];
                if (usedTetrahedra.count(adjIndex)) continue;

                const Tetrahedron& tet2 = tetrahedra[adjIndex];

                // Combine vertices from both tetrahedra to form a hexahedron
                std::set<int> uniqueVertices = {tet1.v1, tet1.v2, tet1.v3, tet1.v4, tet2.v1, tet2.v2, tet2.v3, tet2.v4};
                if (uniqueVertices.size() == 8) { // Valid hexahedron
                    auto it = uniqueVertices.begin();
                    hexahedra.push_back({
                        *it++, *it++, *it++, *it++, *it++, *it++, *it++, *it++
                    });
                    usedTetrahedra.insert(i);
                    usedTetrahedra.insert(adjIndex);
                    break;
                }
            }
        }
    }

    return hexahedra;
}
