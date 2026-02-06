#include "PointQuery.h"

/**
 * @brief Construct a new Point Query:: Point Query object
 * @param mesh Mesh object
 */
PointQuery::PointQuery(const Mesh& mesh)
{
    m_mesh = mesh;
}

/**
 * @brief Get the distance between points
 * @param v1 First point
 * @param v2 Second point
 * @return float Distance value
 */
float getDistanceBetweenPts(const Eigen::Vector3d& v1, const Eigen::Vector3d& v2)
{
    return sqrtf(pow(v2.x()-v1.x(), 2)+pow(v2.y()-v1.y(), 2)+pow(v2.z()-v1.z(), 2));
}

/**
 * @brief Fast way to eliminate vertices further away from given distance
 * // https://en.wikipedia.org/wiki/Taxicab_geometry
 * @param v1 First point
 * @param v2 Second point
 * @param dist Distance to compare
 * @return true If distance in all axis are bigger than query distance
 * @return false If the distance in any of the axis is smallar than query distance
 */
bool isWithin3DManhattanDistance(const Eigen::Vector3d& v1, const Eigen::Vector3d& v2, const float& dist)
{
    float dx = abs(v2.x()-v1.x());
    if (dx > dist) return false;

    float dy = abs(v2.y()-v1.y());
    if (dy > dist) return false;

    float dz = abs(v2.z()-v1.z());
    if (dz > dist) return false;

    return true;
}

/**
 * @brief get the closest vertex to the query point given max radius.
 * if can't find anything else, return query point
 * @param queryPoint Query point
 * @param minDist Search radius
 * @return Eigen::Vector3d Closest point
 */
Eigen::Vector3d PointQuery::getClosestVertex(const Eigen::Vector3d& queryPoint, float& minDist)
{
    // first eliminate vertices further away from max distance using
    // the faster Manhattan distance algorithm since sqrt is expensive
    std::vector<Eigen::Vector3d> filteredVertices;
    for(int i=0; i < m_mesh.getVertices().size(); i++)
    {
        if(isWithin3DManhattanDistance(m_mesh.getVertex(i), queryPoint, minDist))
        {
            filteredVertices.push_back(m_mesh.getVertex(i));
        }
    }

    // now make accurate Euclidean distance comparison with
    // filtered vertices
    Eigen::Vector3d currentClosest = queryPoint;
    for(int i=0; i < filteredVertices.size(); i++)
    {
        float d = getDistanceBetweenPts(filteredVertices[i], queryPoint);
        if(d<minDist)
        {
            minDist = d;
            currentClosest = filteredVertices[i];
        }
    }

    return currentClosest;
}

/**
 * @brief Clamp utility function to restrict value to a given range
 * @param n Value to be clamped
 * @param lower Min value
 * @param upper Max value
 * @return float Clamped Value
 */
float clamp(float n, float lower, float upper) {
    return std::min(lower, std::max(n, upper));
}

/**
 * @brief Get closest point on given face. Face object will be triangular.
 * // https://www.gamedev.net/forums/topic/552906-closest-point-on-triangle/
 * @param face Face object
 * @param queryPoint Query point
 * @return Eigen::Vector3d Closest point
 */
Eigen::Vector3d PointQuery::closestPointOnTriangle(const Face& face, const Eigen::Vector3d& queryPoint)
{
    Eigen::Vector3d edge0 = face.m_v2 - face.m_v1;
    Eigen::Vector3d edge1 = face.m_v3 - face.m_v1;
    Eigen::Vector3d v0 = face.m_v1 - queryPoint;

    float a = edge0.dot( edge0 );
    float b = edge0.dot( edge1 );
    float c = edge1.dot( edge1 );
    float d = edge0.dot( v0 );
    float e = edge1.dot( v0 );

    float det = a*c - b*b;
    float s = b*e - c*d;
    float t = b*d - a*e;

    if ( s + t < det )
    {
        if ( s < 0.f )
        {
            if ( t < 0.f )
            {
                if ( d < 0.f )
                {
                    s = clamp( -d/a, 0.f, 1.f );
                    t = 0.f;
                }
                else
                {
                    s = 0.f;
                    t = clamp( -e/c, 0.f, 1.f );
                }
            }
            else
            {
                s = 0.f;
                t = clamp( -e/c, 0.f, 1.f );
            }
        }
        else if ( t < 0.f )
        {
            s = clamp( -d/a, 0.f, 1.f );
            t = 0.f;
        }
        else
        {
            float invDet = 1.f / det;
            s *= invDet;
            t *= invDet;
        }
    }
    else
    {
        if ( s < 0.f )
        {
            float tmp0 = b+d;
            float tmp1 = c+e;
            if ( tmp1 > tmp0 )
            {
                float numer = tmp1 - tmp0;
                float denom = a-2*b+c;
                s = clamp( numer/denom, 0.f, 1.f );
                t = 1-s;
            }
            else
            {
                t = clamp( -e/c, 0.f, 1.f );
                s = 0.f;
            }
        }
        else if ( t < 0.f )
        {
            if ( a+d > b+e )
            {
                float numer = c+e-b-d;
                float denom = a-2*b+c;
                s = clamp( numer/denom, 0.f, 1.f );
                t = 1-s;
            }
            else
            {
                s = clamp( -e/c, 0.f, 1.f );
                t = 0.f;
            }
        }
        else
        {
            float numer = c+e-b-d;
            float denom = a-2*b+c;
            s = clamp( numer/denom, 0.f, 1.f );
            t = 1.f - s;
        }
    }

    return face.m_v1 + s * edge0 + t * edge1;
}

/**
 * @brief Main query function. First check within object vertices,
 * then check within object faces.
 * @param queryPoint Query point
 * @param maxDist Max radius
 * @return Eigen::Vector3d Closest point
 */
Eigen::Vector3d PointQuery::operator()(const Eigen::Vector3d& queryPoint, float& maxDist)
{

    // first check all vertices
    Eigen::Vector3d result = getClosestVertex(queryPoint, maxDist);

    // next check all faces
    for(int i=0; i<m_mesh.getFaces().size(); i++)
    {
        Eigen::Vector3d closestVertex = closestPointOnTriangle(m_mesh.getFace(i), queryPoint);
        float tmpDist = getDistanceBetweenPts(closestVertex, queryPoint);

        if(maxDist >= tmpDist && closestVertex!=queryPoint)
        {
            result = closestVertex;
            maxDist = tmpDist;
        }

    }
    return result;
}