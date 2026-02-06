#ifndef POINTQUERY_H
#define POINTQUERY_H

#include "Mesh.h"

/**
 * @brief PointQuery class; contains query functions and mesh object.
 */
class PointQuery
{
private:
    Mesh m_mesh;
public:
    PointQuery(const Mesh& mesh);
    ~PointQuery() {};

    Eigen::Vector3d getClosestVertex(const Eigen::Vector3d& queryPoint, float& minDist);
    Eigen::Vector3d closestPointOnTriangle(const Face& face, const Eigen::Vector3d& queryPoint);
    Eigen::Vector3d operator() (const Eigen::Vector3d& queryPoint, float& maxDist);
};

#endif // POINTQUERY_H