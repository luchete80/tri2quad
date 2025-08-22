#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <fstream>
#include <map>
#include <cmath>
using namespace std;

// ---------------------------
// Estructuras de datos
// ---------------------------
struct Vertex { double x,y,z; };
struct Triangle { int v1,v2,v3; };
struct Quad { int v1,v2,v3,v4; };

// ---------------------------
// Hash para std::pair<int,int>
// ---------------------------
struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1,T2>& p) const {
        return std::hash<T1>()(p.first) ^ (std::hash<T2>()(p.second)<<1);
    }
};

inline pair<int,int> makeEdgeKey(int a,int b){ return minmax(a,b); }

// ---------------------------
// Función de calidad de quad
// ---------------------------
double quadQuality(const vector<Vertex>& verts, const Quad& q){
    auto dist2D = [](const Vertex& a, const Vertex& b){
        double dx=a.x-b.x, dy=a.y-b.y; return sqrt(dx*dx+dy*dy);
    };
    double l1=dist2D(verts[q.v1],verts[q.v2]);
    double l2=dist2D(verts[q.v2],verts[q.v3]);
    double l3=dist2D(verts[q.v3],verts[q.v4]);
    double l4=dist2D(verts[q.v4],verts[q.v1]);
    double side_ratio = min(l1,l3)/max(l1,l3) * min(l2,l4)/max(l2,l4);

    auto angleCos = [&](Vertex a,Vertex b,Vertex c){
        double dx1=a.x-b.x, dy1=a.y-b.y;
        double dx2=c.x-b.x, dy2=c.y-b.y;
        double dot=dx1*dx2+dy1*dy2;
        double len1=sqrt(dx1*dx1+dy1*dy1);
        double len2=sqrt(dx2*dx2+dy2*dy2);
        return dot/(len1*len2+1e-12);
    };
    double angles = fabs(angleCos(verts[q.v4],verts[q.v1],verts[q.v2])) +
                    fabs(angleCos(verts[q.v1],verts[q.v2],verts[q.v3])) +
                    fabs(angleCos(verts[q.v2],verts[q.v3],verts[q.v4])) +
                    fabs(angleCos(verts[q.v3],verts[q.v4],verts[q.v1]));
    double angle_score = 1.0/(angles+1e-6);
    return side_ratio*angle_score;
}

// ---------------------------
// Convertir triángulos a quads
// ---------------------------
vector<Quad> convertToQuads(const vector<Vertex>& vertices, const vector<Triangle>& triangles){
    unordered_map<pair<int,int>, vector<int>, PairHash> edgeToTriangles;
    for(size_t i=0;i<triangles.size();++i){
        const Triangle& tri=triangles[i];
        for(auto edge : {makeEdgeKey(tri.v1,tri.v2), makeEdgeKey(tri.v2,tri.v3), makeEdgeKey(tri.v3,tri.v1)})
            edgeToTriangles[edge].push_back(i);
    }

    set<int> usedTriangles;
    vector<Quad> quads;

    for(size_t i=0;i<triangles.size();++i){
        if(usedTriangles.count(i)) continue;
        const Triangle& tri1=triangles[i];

        Quad bestQuad;
        double bestQuality=-1.0;
        int bestNeighbor=-1;

        for(auto edge : {makeEdgeKey(tri1.v1,tri1.v2), makeEdgeKey(tri1.v2,tri1.v3), makeEdgeKey(tri1.v3,tri1.v1)}){
            const auto& adjList = edgeToTriangles[edge];
            if(adjList.size()!=2) continue;
            int adjIndex = (adjList[0]==i)? adjList[1]: adjList[0];
            if(usedTriangles.count(adjIndex)) continue;

            const Triangle& tri2=triangles[adjIndex];

            vector<int> shared, unshared;
            for(int v : {tri1.v1,tri1.v2,tri1.v3})
                if(v==tri2.v1 || v==tri2.v2 || v==tri2.v3) shared.push_back(v);
                else unshared.push_back(v);
            for(int v : {tri2.v1,tri2.v2,tri2.v3})
                if(find(shared.begin(),shared.end(),v)==shared.end()) unshared.push_back(v);

            if(shared.size()==2 && unshared.size()==2){
                Quad candidate{unshared[0], shared[0], unshared[1], shared[1]};
                double q = quadQuality(vertices,candidate);
                if(q>bestQuality){ bestQuality=q; bestQuad=candidate; bestNeighbor=adjIndex; }
            }
        }

        if(bestNeighbor!=-1){
            quads.push_back(bestQuad);
            usedTriangles.insert(i);
            usedTriangles.insert(bestNeighbor);
        }
    }
    return quads;
}

// ---------------------------
// Lectura archivo Gmsh
// ---------------------------
void readGmshFile(const string& filename, vector<Vertex>& vertices, vector<Triangle>& triangles){
    ifstream file(filename);
    if(!file.is_open()){ cerr<<"Cannot open "<<filename<<"\n"; return; }
    map<int,int> vertexMap;
    string line;
    while(getline(file,line)){
        if(line=="$Nodes"){
            int numNodes; file>>numNodes; vertices.resize(numNodes);
            for(int i=0;i<numNodes;++i){
                int id; file>>id>>vertices[i].x>>vertices[i].y>>vertices[i].z;
                vertexMap[id]=i;
            }
        } else if(line=="$Elements"){
            int numElements; file>>numElements;
            for(int i=0;i<numElements;++i){
                int id,type,tags; file>>id>>type>>tags;
                for(int j=0;j<tags;++j){ int dummy; file>>dummy; } // ignorar tags
                if(type==2){ // triángulo
                    int v[3]; file>>v[0]>>v[1]>>v[2];
                    triangles.push_back({vertexMap[v[0]], vertexMap[v[1]], vertexMap[v[2]]});
                } else file.ignore(numeric_limits<streamsize>::max(),'\n');
            }
        }
    }
    file.close();
}

// ---------------------------
// Escritura archivo Gmsh
// ---------------------------
void writeGmshFile(const string& filename, const vector<Vertex>& vertices, const vector<Quad>& quads){
    ofstream file(filename);
    if(!file.is_open()){ cerr<<"Cannot open "<<filename<<"\n"; return; }
    file<<"$MeshFormat\n2.2 0 8\n$EndMeshFormat\n";
    file<<"$Nodes\n"<<vertices.size()<<"\n";
    for(size_t i=0;i<vertices.size();++i)
        file<<i+1<<" "<<vertices[i].x<<" "<<vertices[i].y<<" "<<vertices[i].z<<"\n";
    file<<"$EndNodes\n";
    file<<"$Elements\n"<<quads.size()<<"\n";
    for(size_t i=0;i<quads.size();++i)
        file<<i+1<<" 3 0 "<<quads[i].v1+1<<" "<<quads[i].v2+1<<" "<<quads[i].v3+1<<" "<<quads[i].v4+1<<"\n";
    file<<"$EndElements\n";
    file.close();
}

// ---------------------------
// Main
// ---------------------------
int main(){
    vector<Vertex> vertices;
    vector<Triangle> triangles;

    readGmshFile("input.msh", vertices, triangles);
    auto quads = convertToQuads(vertices, triangles);

    cout<<"Generated "<<quads.size()<<" quads from "<<triangles.size()<<" triangles\n";

    writeGmshFile("output.msh", vertices, quads);

    return 0;
}
