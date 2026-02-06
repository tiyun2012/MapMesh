// needed once for tinyobjloader
#define TINYOBJLOADER_IMPLEMENTATION

#include <fstream>
#include "stdio.h"
#include "PointQuery.h"

int main(int argc, char** argv)
{
    Mesh mesh;
    const char * objFile = "../data/teapot.obj";
    const char * pointQueryFile = "../data/teapot_pts.txt";

    if(argc>2)
    {
        objFile = argv[1];
        pointQueryFile = argv[2];
    }
    else
    {
        printf("No .obj file and/or query .txt file provided, using default test case\n");
    }

    // read obj file
    if(mesh.readObj(objFile))
    {
        PointQuery query(mesh);
        std::ifstream input(pointQueryFile);
        float x, y, z, querydist, dist;

        // read input query pts & distance
        while(input >> x >> y >> z >> querydist)
        {
            std::cout << "===============================================" << std::endl;
            dist = querydist;
            Eigen::Vector3d queryPoint(x, y, z);
            Eigen::Vector3d result = query(queryPoint, dist);
            if(result != queryPoint)
            {
                printf("FOUND pt: %f %f %f within distance: %f to query pt: %f %f %f max search radius: %f\n", result.x(), result.y(), result.z(), dist, x, y, z, querydist);
            }
            else
            {
                printf("NOT FOUND pt within distance %f to query pt %f %f %f\n", dist, x, y, z);
            }
        }
    }
}