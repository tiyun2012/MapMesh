#pragma once

#include <maya/MPxNode.h>
#include <maya/MObject.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>

class NeighboringVerticesNode : public MPxNode {
public:
    NeighboringVerticesNode() = default;
    ~NeighboringVerticesNode() override = default;

    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static MTypeId id;

    // Inputs
    static MObject aInMesh;       // mesh input
    static MObject aWorldMatrix;  // mesh world matrix
    static MObject aPosition;     // world position (double3)
    static MObject aDistance;     // radius distance
    static MObject aFalloff;      // enum: volume or surface

    // Output
    static MObject aOutVertexIds; // int array of vertex ids
    static MObject aOutCount;     // number of vertices found
    static MObject aClosestPoint; // closest point position (world)
    static MObject aClosestVertexId; // closest vertex id
    static MObject aClosestUV;    // closest point UV (double2)
    static MObject aClosestFaceId; // closest face id
};
