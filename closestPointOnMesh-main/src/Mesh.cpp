#include "Mesh.h"
#include "stdio.h"

/**
 * @brief Add given vertex to vertices vector
 * @param vertex Eigen::Vector3d point to be added
 */
void Mesh::addVertex(const Eigen::Vector3d& vertex)
{
    m_vertices.push_back(vertex);
}

/**
 * @brief Get point data to init Eigen::Vector3d vertex, later to be added in vertices vector
 * Polymorphed function added for readability
 * @param x X axis coord
 * @param y Y axis coord
 * @param z Z axis coord
 */
void Mesh::addVertex(const double x, const double y, const double z)
{
    addVertex(Eigen::Vector3d(x, y, z));
}

/**
 * @brief Add given face to faces vector
 * @param face Face object
 */
void Mesh::addFace(const Face& face)
{
    m_faces.push_back(face);
}

/**
 * @brief Get vertices to init Face objects, later to be added in faces vector
 * Polymorphed function added for readability
 * @param id Face id
 * @param v1 Point 1
 * @param v2 Point 2
 * @param v3 Point 3
 */
void Mesh::addFace(const int& id, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2, const Eigen::Vector3d& v3)
{
    addFace(Face(id, v1, v2, v3));
}

/**
 * @brief Read-in .obj file and serialize mesh data in vertices and faces vectors
 * // https://github.com/tinyobjloader/tinyobjloader/blob/master/loader_example.cc
 * Assuming we are using triangulated mesh.
 * @param filename Full file path
 * @return true Successful .obj load
 * @return false Fail if can't load .obj
 */
bool Mesh::readObj(const char* filename)
{
    bool triangulate = true;
    const char* basepath = NULL;

    // load obj file
    std::cout << "Loading obj file: " << filename << std::endl;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;
    bool response = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

    if (!warn.empty()) {
        std::cout << "OBJ LOAD WARNING: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "OBJ LOAD ERROR: " << err << std::endl;
    }

    if (!response) {
        printf("Failed to load/parse .obj\n");
        return false;
    }

    // add each vertex
    for (size_t i = 0; i<attrib.vertices.size()/3; i++)
    {
        addVertex(
            static_cast<const double>(attrib.vertices[3 * i + 0]),
            static_cast<const double>(attrib.vertices[3 * i + 1]),
            static_cast<const double>(attrib.vertices[3 * i + 2]));
    }

    // for each shape
    for (size_t i = 0; i < shapes.size(); i++)
    {
        size_t index_offset = 0;

        // for each face
        for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++)
        {
            size_t fnum = shapes[i].mesh.num_face_vertices[f];

            // assuming faces to be triangles
            int v1_id = shapes[i].mesh.indices[index_offset + 0].vertex_index;
            int v2_id = shapes[i].mesh.indices[index_offset + 1].vertex_index;
            int v3_id = shapes[i].mesh.indices[index_offset + 2].vertex_index;

            addFace(f, getVertex(v1_id), getVertex(v2_id), getVertex(v3_id));
            index_offset += fnum;
        }
    }
    printf("Obj loaded succesfully. Vertex count: %lu. Face count: %lu\n", getVertices().size(), getFaces().size());
    return true;

}