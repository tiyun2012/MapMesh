#ifndef MESH_H
#define MESH_H

#include <iostream>
#include <vector>
#include <Eigen3/Eigen/Dense>
#include "tiny_obj_loader.h"

/**
 * @brief Face class; contains 3 vertices which forms the face and face id.
 */
struct Face
{
public:
    int m_id;
    Eigen::Vector3d m_v1, m_v2, m_v3;
    Face(const int& id, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2, const Eigen::Vector3d& v3){
        m_id = id;
        m_v1=v1;
        m_v2=v2;
        m_v3=v3;
    };
};

/**
 * @brief Mesh class; contains std::vectors of vertices and faces of the .obj
 * Getter/setter functions needed to populate mesh object and readObj function
 * to read in mesh data from .obj file
 */
class Mesh
{
private:
    std::vector<Eigen::Vector3d> m_vertices;
    std::vector<Face> m_faces;

public:
    Mesh() {};
    ~Mesh() {};

    inline Eigen::Vector3d& getVertex(const int& id) {return m_vertices[id];};
    inline std::vector<Eigen::Vector3d> getVertices() {return m_vertices;};
    void addVertex(const Eigen::Vector3d& vertex);
    void addVertex(const double x, const double y, const double z);

    inline Face getFace(const int& id) {return m_faces[id];};
    inline std::vector<Face> getFaces() {return m_faces;};
    void addFace(const Face& face);
    void addFace(const int& id, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2, const Eigen::Vector3d& v3);

    bool readObj(const char* filename);
};

#endif // MESH_H