#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <set>
#include <algorithm>

#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
using namespace std;
// Define structures for vertices, triangles, and quads
struct Vertex {
    double x, y, z;
};

struct Triangle {
    int v1, v2, v3; // Indices of vertices
};

struct Quad {
    int v1, v2, v3, v4; // Indices of vertices
};

// Custom hash function for std::pair
struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& pair) const {
        return std::hash<T1>()(pair.first) ^ (std::hash<T2>()(pair.second) << 1);
    }
};

// Utility to create an edge key
std::pair<int, int> makeEdgeKey(int v1, int v2) {
    return std::minmax(v1, v2); // Ensure consistent order for the edge
}

// Function to convert triangles to quads
std::vector<Quad> convertToQuads(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles) {
    // Map to store adjacent triangles for each edge
    std::unordered_map<std::pair<int, int>, std::vector<int>, PairHash> edgeToTriangles;

    // Fill edge-to-triangle map
    for (size_t i = 0; i < triangles.size(); ++i) {
        const Triangle& tri = triangles[i];
        auto edges = {makeEdgeKey(tri.v1, tri.v2), makeEdgeKey(tri.v2, tri.v3), makeEdgeKey(tri.v3, tri.v1)};
        for (const auto& edge : edges) {
            edgeToTriangles[edge].push_back(i);
        }
    }

    // Set to track used triangles
    std::set<int> usedTriangles;
    std::vector<Quad> quads;
    
    int shared_edge_count=0;
    // Iterate over triangles and form quads
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (usedTriangles.count(i)) continue; // Skip already used triangles

        const Triangle& tri1 = triangles[i];
        for (const auto& edge : {makeEdgeKey(tri1.v1, tri1.v2), makeEdgeKey(tri1.v2, tri1.v3), makeEdgeKey(tri1.v3, tri1.v1)}) {
            const auto& adjacentTriangles = edgeToTriangles[edge];
            if (adjacentTriangles.size() == 2) { // Only consider edges shared by exactly two triangles
                int adjIndex = adjacentTriangles[0] == i ? adjacentTriangles[1] : adjacentTriangles[0];
                if (usedTriangles.count(adjIndex)) continue;

                const Triangle& tri2 = triangles[adjIndex];

                // Find the shared edge vertices
                std::vector<int> shared, unshared;
                for (int v : {tri1.v1, tri1.v2, tri1.v3}) {
                    if (v == tri2.v1 || v == tri2.v2 || v == tri2.v3) {
                        shared.push_back(v);
                    } else {
                        unshared.push_back(v);
                    }
                }
                for (int v : {tri2.v1, tri2.v2, tri2.v3}) {
                    if (std::find(shared.begin(), shared.end(), v) == shared.end()) {
                        unshared.push_back(v);
                    }
                }/*
                cout << "Tri "<<i<<", shared tri"<<adjIndex<<endl;
                cout << "Tri Verts "<<tri1.v1 <<","<<tri1.v2<<","<<tri1.v3<<endl;
                cout << "Shared Tri Verts "<<tri2.v1<<","<<tri2.v2<<","<<tri2.v3<<endl;
                cout <<"  Shared verts size "<<shared.size()<<endl;
                cout <<"UnShared verts size "<<shared.size()<<endl;
                */
                if (shared.size() == 2 && unshared.size() == 2) { // Valid quad
                    quads.push_back({unshared[0], shared[0], unshared[1], shared[1]});
                    usedTriangles.insert(i);
                    usedTriangles.insert(adjIndex);
                    break;
                }
              shared_edge_count++;
            }
        }
    }
    cout << "Shared edges"<<shared_edge_count<<endl;
    return quads;
}

// Function to read Gmsh file
void readGmshFile(const std::string& filename, std::vector<Vertex>& vertices, std::vector<Triangle>& triangles) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return;
    }

  // we make a map between serial number of the vertex and its number in the file.
  // it will help us when we create mesh elements
  std::map<int, int> vertices_map;
  
    std::string line;
    while (std::getline(file, line)) {
        if (line == "$Nodes") {
            int numNodes;
            file >> numNodes;
            vertices.resize(numNodes);
            for (int i = 0; i < numNodes; ++i) {
                int id;
                file >> id >> vertices[i].x >> vertices[i].y >> vertices[i].z;
                vertices_map[id] = i; // add the number of vertex to the map
            }
        } else if (line == "$Elements") {
            int numElements;
            file >> numElements;
            for (int i = 0; i < numElements; ++i) {
                int id, type, tags;
                file >> id >> type >> tags;

                std::vector<int> data(tags); // allocate the memory for some data
                for (int i = 0; i <tags; ++i) // read this information
                  file >> data[i];
                int phys_domain = (tags > 0) ? data[0] : 0; // physical domain - the most important value
      //          elem_domain = (n_tags > 1) ? data[1] : 0; // elementary domain
      //          partition   = (n_tags > 2) ? data[2] : 0; // partition (Metis, Chaco, etc)
                data.clear(); // other data isn't interesting for us

                if (type == 2) { // Triangle
                    Triangle tri;
                    //file >> tri.v1 >> tri.v2 >> tri.v3;
                    int vid;
                    file >> vid;tri.v1= vertices_map[vid];
                    file >> vid;tri.v2= vertices_map[vid];                    
                    file >> vid;tri.v3= vertices_map[vid];
                    
                    triangles.push_back(tri);
                } else {
                    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
            }
        }
    }
    cout << "Readed "<<triangles.size()<<" triangles"<<endl;
    file.close();
}


// Function to write Gmsh file
void writeGmshFile(const std::string& filename, const std::vector<Vertex>& vertices, const std::vector<Quad>& quads) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return;
    }

    file << "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n";
    file << "$Nodes\n" << vertices.size() << "\n";
    for (size_t i = 0; i < vertices.size(); ++i) {
        file << i + 1 << " " << vertices[i].x << " " << vertices[i].y << " " << vertices[i].z << "\n";
    }
    file << "$EndNodes\n";

    file << "$Elements\n" << quads.size() << "\n";
    for (size_t i = 0; i < quads.size(); ++i) {
        file << i + 1 << " 3 0 " << quads[i].v1 + 1 << " " << quads[i].v2 + 1 << " " 
             << quads[i].v3 + 1 << " " << quads[i].v4 + 1 << "\n";
    }
    file << "$EndElements\n";

    file.close();
}

/*
// Example usage
int main() {
    // Define a simple triangular mesh
    std::vector<Vertex> vertices = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.5, 1.0, 0.0}, 
        {1.5, 1.0, 0.0}, {2.0, 0.0, 0.0}, {1.0, -1.0, 0.0}
    };
    std::vector<Triangle> triangles = {
        {0, 1, 2}, {1, 3, 2}, {1, 4, 3}, {1, 5, 4}
    };

    // Convert to quads
    auto quads = convertToQuads(vertices, triangles);

    // Output results
    std::cout << "Quads: \n";
    for (const auto& quad : quads) {
        std::cout << quad.v1 << ", " << quad.v2 << ", " << quad.v3 << ", " << quad.v4 << "\n";
    }

    return 0;
}


*/

int main() {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;

    // Read Gmsh file
    readGmshFile("input.msh", vertices, triangles);

    // Convert to quads
    auto quads = convertToQuads(vertices, triangles);
    cout << "generated "<<quads.size()<<" quads"<<endl;
    // Write Gmsh file
    writeGmshFile("output.msh", vertices, quads);

    std::cout << "Converted triangles to quads and wrote to output.msh\n";

    return 0;
}