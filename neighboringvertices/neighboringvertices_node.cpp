// neighboringvertices_node.cpp
// Outputs vertex ids within a distance from the closest point to a world position.

#include "neighboringvertices_node.h"

#include <maya/MFnPlugin.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnMesh.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MMeshIntersector.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MString.h>
#include <maya/MPlugArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MDagPath.h>
#include <maya/MFloatArray.h>

#include <queue>
#include <vector>
#include <string>
#include <limits>
#include <functional>

MTypeId NeighboringVerticesNode::id(0x0012F2A3);

MObject NeighboringVerticesNode::aInMesh;
MObject NeighboringVerticesNode::aWorldMatrix;
MObject NeighboringVerticesNode::aPosition;
MObject NeighboringVerticesNode::aDistance;
MObject NeighboringVerticesNode::aFalloff;
MObject NeighboringVerticesNode::aOutVertexIds;
MObject NeighboringVerticesNode::aOutCount;
MObject NeighboringVerticesNode::aClosestPoint;
MObject NeighboringVerticesNode::aClosestVertexId;
MObject NeighboringVerticesNode::aClosestUV;
MObject NeighboringVerticesNode::aClosestFaceId;

void* NeighboringVerticesNode::creator() {
    return new NeighboringVerticesNode();
}

MStatus NeighboringVerticesNode::initialize() {
    MStatus status;

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnEnumAttribute eAttr;
    MFnMatrixAttribute mAttr;

    aInMesh = tAttr.create("inMesh", "inm", MFnData::kMesh, MObject::kNullObj, &status);
    if (status != MS::kSuccess) return status;
    tAttr.setKeyable(false);
    tAttr.setStorable(false);
    tAttr.setReadable(true);
    tAttr.setWritable(true);
    addAttribute(aInMesh);

    aWorldMatrix = mAttr.create("worldMatrix", "wm", MFnMatrixAttribute::kDouble, &status);
    if (status != MS::kSuccess) return status;
    mAttr.setKeyable(false);
    mAttr.setStorable(false);
    mAttr.setReadable(true);
    mAttr.setWritable(true);
    addAttribute(aWorldMatrix);

    aPosition = nAttr.createPoint("position", "pos", &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    addAttribute(aPosition);

    aDistance = nAttr.create("distance", "dist", MFnNumericData::kDouble, 0.0, &status);
    if (status != MS::kSuccess) return status;
    nAttr.setMin(0.0);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    addAttribute(aDistance);

    aFalloff = eAttr.create("falloff", "fall", 0, &status);
    if (status != MS::kSuccess) return status;
    eAttr.addField("volume", 0);
    eAttr.addField("surface", 1);
    eAttr.setKeyable(true);
    eAttr.setStorable(true);
    addAttribute(aFalloff);

    aOutVertexIds = tAttr.create("outVertexIds", "outv", MFnData::kIntArray, MObject::kNullObj, &status);
    if (status != MS::kSuccess) return status;
    tAttr.setKeyable(false);
    tAttr.setStorable(false);
    tAttr.setReadable(true);
    tAttr.setWritable(false);
    addAttribute(aOutVertexIds);

    aOutCount = nAttr.create("outCount", "outc", MFnNumericData::kInt, 0, &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(aOutCount);

    aClosestPoint = nAttr.createPoint("closestPoint", "cpos", &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(aClosestPoint);

    aClosestVertexId = nAttr.create("closestVertexId", "cvid", MFnNumericData::kInt, -1, &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(aClosestVertexId);

    aClosestUV = nAttr.create("closestUV", "cuv", MFnNumericData::k2Double, 0.0, &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(aClosestUV);

    aClosestFaceId = nAttr.create("closestFaceId", "cfid", MFnNumericData::kInt, -1, &status);
    if (status != MS::kSuccess) return status;
    nAttr.setKeyable(false);
    nAttr.setStorable(false);
    nAttr.setReadable(true);
    nAttr.setWritable(false);
    addAttribute(aClosestFaceId);

    attributeAffects(aInMesh, aOutVertexIds);
    attributeAffects(aWorldMatrix, aOutVertexIds);
    attributeAffects(aPosition, aOutVertexIds);
    attributeAffects(aDistance, aOutVertexIds);
    attributeAffects(aFalloff, aOutVertexIds);
    attributeAffects(aInMesh, aOutCount);
    attributeAffects(aWorldMatrix, aOutCount);
    attributeAffects(aPosition, aOutCount);
    attributeAffects(aDistance, aOutCount);
    attributeAffects(aFalloff, aOutCount);
    attributeAffects(aInMesh, aClosestPoint);
    attributeAffects(aWorldMatrix, aClosestPoint);
    attributeAffects(aPosition, aClosestPoint);
    attributeAffects(aDistance, aClosestPoint);
    attributeAffects(aFalloff, aClosestPoint);
    attributeAffects(aInMesh, aClosestVertexId);
    attributeAffects(aWorldMatrix, aClosestVertexId);
    attributeAffects(aPosition, aClosestVertexId);
    attributeAffects(aDistance, aClosestVertexId);
    attributeAffects(aFalloff, aClosestVertexId);
    attributeAffects(aInMesh, aClosestUV);
    attributeAffects(aWorldMatrix, aClosestUV);
    attributeAffects(aPosition, aClosestUV);
    attributeAffects(aDistance, aClosestUV);
    attributeAffects(aFalloff, aClosestUV);
    attributeAffects(aInMesh, aClosestFaceId);
    attributeAffects(aWorldMatrix, aClosestFaceId);
    attributeAffects(aPosition, aClosestFaceId);
    attributeAffects(aDistance, aClosestFaceId);
    attributeAffects(aFalloff, aClosestFaceId);

    return MS::kSuccess;
}

MStatus NeighboringVerticesNode::compute(const MPlug& plug, MDataBlock& data) {
    auto isPlugOrChild = [&](const MObject& attr) {
        return plug == attr || plug.parent() == attr;
    };
    if (!isPlugOrChild(aOutVertexIds) &&
        !isPlugOrChild(aOutCount) &&
        !isPlugOrChild(aClosestPoint) &&
        !isPlugOrChild(aClosestVertexId) &&
        !isPlugOrChild(aClosestUV) &&
        !isPlugOrChild(aClosestFaceId))
        return MS::kUnknownParameter;

    MStatus status;
    MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
    if (status != MS::kSuccess) return status;
    MObject meshObj = inMeshHandle.asMesh();

    // Prepare empty output by default.
    MFnIntArrayData outDataFn;
    MIntArray outIds;
    int closestVertexId = -1;
    int closestFaceId = -1;
    MPoint closestWorld(0.0, 0.0, 0.0);
    double closestU = 0.0;
    double closestV = 0.0;

    if (meshObj.isNull()) {
        MObject outData = outDataFn.create(outIds, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        data.outputValue(aOutVertexIds).setMObject(outData);
        data.outputValue(aOutCount).setInt(0);
        data.outputValue(aClosestPoint).set3Double(0.0, 0.0, 0.0);
        data.outputValue(aClosestVertexId).setInt(-1);
        data.outputValue(aClosestUV).set2Double(0.0, 0.0);
        data.outputValue(aClosestFaceId).setInt(-1);
        data.setClean(plug);
        return MS::kSuccess;
    }

    const MMatrix worldMatrix = data.inputValue(aWorldMatrix, &status).asMatrix();
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double3& pos = data.inputValue(aPosition, &status).asDouble3();
    CHECK_MSTATUS_AND_RETURN_IT(status);
    double distance = data.inputValue(aDistance, &status).asDouble();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    if (distance < 0.0)
        distance = 0.0;

    const MPoint posWorld(pos[0], pos[1], pos[2]);
    const short falloff = data.inputValue(aFalloff, &status).asShort();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MFnMesh fnMesh(meshObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Try to resolve a DAG path from the inMesh connection (for optional matrix fallback).
    MDagPath meshPath;
    bool haveMeshPath = false;
    bool meshIsWorldSpace = false;
    {
        MPlug inMeshPlug(thisMObject(), aInMesh);
        MPlugArray sources;
        inMeshPlug.connectedTo(sources, true, false);
        for (unsigned int i = 0; i < sources.length(); ++i) {
            MPlug src = sources[i];
            MObject srcNode = src.node();
            if (!srcNode.hasFn(MFn::kMesh))
                continue;
            if (MDagPath::getAPathTo(srcNode, meshPath) != MS::kSuccess)
                continue;
            haveMeshPath = true;

            // Detect if source plug is worldMesh (optional hint).
            MFnAttribute attr(src.attribute());
            if (attr.name() == "worldMesh")
                meshIsWorldSpace = true;
            const std::string srcName = src.name().asChar();
            if (srcName.find(".worldMesh") != std::string::npos)
                meshIsWorldSpace = true;
            break;
        }
    }

    MMatrix meshToWorld = worldMatrix;
    if (haveMeshPath) {
        if (meshIsWorldSpace) {
            meshToWorld = MMatrix::identity;
        } else {
            // Use the actual mesh DAG transform when possible (matches Maya sample).
            meshToWorld = meshPath.inclusiveMatrix();
        }
    }

    MPoint closestObj;
    int faceId = -1;

    int triIndex = -1;
    // Use MMeshIntersector (matches closestPointCmd behavior).
    {
        MMeshIntersector intersector;
        status = intersector.create(meshObj, meshToWorld);
        if (status == MS::kSuccess) {
            MPointOnMesh pointInfo;
            status = intersector.getClosestPoint(posWorld, pointInfo);
            if (status == MS::kSuccess) {
                closestObj = pointInfo.getPoint(); // object space
                closestWorld = closestObj * meshToWorld; // world space
                faceId = pointInfo.faceIndex();
                closestFaceId = faceId;
                triIndex = pointInfo.triangleIndex();
            }
        }
    }

    if (status != MS::kSuccess) {
        // Fallback to MFnMesh if intersector failed.
        const MMatrix invWorld = meshToWorld.inverse();
        const MPoint posObj = posWorld * invWorld;
        status = fnMesh.getClosestPoint(posObj, closestObj, MSpace::kObject, &faceId);
        if (status == MS::kSuccess) {
            closestWorld = closestObj * meshToWorld;
            closestFaceId = faceId;
        }
    }

    if (status != MS::kSuccess) {
        MObject outData = outDataFn.create(outIds, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        data.outputValue(aOutVertexIds).setMObject(outData);
        data.outputValue(aOutCount).setInt(0);
        data.outputValue(aClosestPoint).set3Double(posWorld.x, posWorld.y, posWorld.z);
        data.outputValue(aClosestVertexId).setInt(-1);
        data.outputValue(aClosestUV).set2Double(0.0, 0.0);
        data.outputValue(aClosestFaceId).setInt(-1);
        data.setClean(plug);
        return MS::kSuccess;
    }

    // Closest UV at closest point.
    {
        bool uvFound = false;
        MString uvSet;
        fnMesh.getCurrentUVSetName(uvSet);

        if (faceId >= 0) {
            MStatus polyStatus;
            MItMeshPolygon polyIt(meshObj, &polyStatus);
            if (polyStatus == MS::kSuccess) {
                int prevIndex = 0;
                if (polyIt.setIndex(faceId, prevIndex) == MS::kSuccess) {
                    MIntArray faceVerts;
                    polyIt.getVertices(faceVerts);

                    MFloatArray uArray, vArray;
                    if (polyIt.getUVs(uArray, vArray, &uvSet) == MS::kSuccess &&
                        uArray.length() == vArray.length() &&
                        uArray.length() == faceVerts.length()) {

                        auto mapTriangleUVs = [&](const MIntArray& triVertIds,
                                                  double& outU0, double& outV0,
                                                  double& outU1, double& outV1,
                                                  double& outU2, double& outV2) -> bool {
                            float uvs[3] = {0.0f, 0.0f, 0.0f};
                            float vvs[3] = {0.0f, 0.0f, 0.0f};
                            for (unsigned int i = 0; i < 3; ++i) {
                                int localIndex = -1;
                                for (unsigned int j = 0; j < faceVerts.length(); ++j) {
                                    if (faceVerts[j] == triVertIds[i]) {
                                        localIndex = static_cast<int>(j);
                                        break;
                                    }
                                }
                                if (localIndex < 0 || localIndex >= static_cast<int>(uArray.length()))
                                    return false;
                                uvs[i] = uArray[localIndex];
                                vvs[i] = vArray[localIndex];
                            }
                            outU0 = uvs[0];
                            outV0 = vvs[0];
                            outU1 = uvs[1];
                            outV1 = vvs[1];
                            outU2 = uvs[2];
                            outV2 = vvs[2];
                            return true;
                        };

                        // Prefer triangle index from MMeshIntersector.
                        if (triIndex >= 0) {
                            MPointArray triPts;
                            MIntArray triVertIds;
                            if (polyIt.getTriangle(triIndex, triPts, triVertIds, MSpace::kObject) == MS::kSuccess &&
                                triPts.length() == 3 && triVertIds.length() == 3) {
                                double u0, v0, u1, v1, u2, v2;
                                if (mapTriangleUVs(triVertIds, u0, v0, u1, v1, u2, v2)) {
                                    const MVector v0p = triPts[1] - triPts[0];
                                    const MVector v1p = triPts[2] - triPts[0];
                                    const MVector v2p = closestObj - triPts[0];
                                    const double d00 = v0p * v0p;
                                    const double d01 = v0p * v1p;
                                    const double d11 = v1p * v1p;
                                    const double d20 = v2p * v0p;
                                    const double d21 = v2p * v1p;
                                    const double denom = d00 * d11 - d01 * d01;
                                    if (std::abs(denom) > 1e-12) {
                                        const double v = (d11 * d20 - d01 * d21) / denom;
                                        const double w = (d00 * d21 - d01 * d20) / denom;
                                        const double u = 1.0 - v - w;
                                        closestU = u0 * u + u1 * v + u2 * w;
                                        closestV = v0 * u + v1 * v + v2 * w;
                                        uvFound = true;
                                    }
                                }
                            }
                        }

                        if (!uvFound) {
                            int triCount = 0;
                            polyIt.numTriangles(triCount);
                            const double eps = 1e-6;
                            double bestDist = std::numeric_limits<double>::max();
                            double bestU = 0.0;
                            double bestV = 0.0;

                            for (int t = 0; t < triCount; ++t) {
                                MPointArray triPts;
                                MIntArray triVertIds;
                                if (polyIt.getTriangle(t, triPts, triVertIds, MSpace::kObject) != MS::kSuccess)
                                    continue;
                                if (triPts.length() != 3 || triVertIds.length() != 3)
                                    continue;

                                const MVector v0 = triPts[1] - triPts[0];
                                const MVector v1 = triPts[2] - triPts[0];
                                const MVector v2 = closestObj - triPts[0];
                                const double d00 = v0 * v0;
                                const double d01 = v0 * v1;
                                const double d11 = v1 * v1;
                                const double d20 = v2 * v0;
                                const double d21 = v2 * v1;
                                const double denom = d00 * d11 - d01 * d01;
                                if (std::abs(denom) < 1e-12)
                                    continue;
                                const double v = (d11 * d20 - d01 * d21) / denom;
                                const double w = (d00 * d21 - d01 * d20) / denom;
                                const double u = 1.0 - v - w;

                                const double dist = std::abs((closestObj - triPts[0]) * (v0 ^ v1).normal());

                                double u0, v0uv, u1, v1uv, u2, v2uv;
                                if (!mapTriangleUVs(triVertIds, u0, v0uv, u1, v1uv, u2, v2uv))
                                    continue;

                                const bool inside = (u >= -eps && v >= -eps && w >= -eps);
                                if (inside || dist < bestDist) {
                                    bestDist = dist;
                                    bestU = u0 * u + u1 * v + u2 * w;
                                    bestV = v0uv * u + v1uv * v + v2uv * w;
                                    uvFound = true;
                                    if (inside)
                                        break;
                                }
                            }

                            if (uvFound) {
                                closestU = bestU;
                                closestV = bestV;
                            }
                        }
                    }
                }
            }
        }

        if (!uvFound) {
            float2 uv = {0.0f, 0.0f};
            if (fnMesh.getUVAtPoint(closestObj, uv, MSpace::kObject, &uvSet) == MS::kSuccess) {
                closestU = static_cast<double>(uv[0]);
                closestV = static_cast<double>(uv[1]);
            }
        }
    }

    // Precompute world-space vertex positions.
    MPointArray objPoints;
    status = fnMesh.getPoints(objPoints, MSpace::kObject);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const unsigned int numVerts = objPoints.length();
    std::vector<MPoint> worldPoints;
    worldPoints.reserve(numVerts);
    for (unsigned int i = 0; i < numVerts; ++i) {
        worldPoints.push_back(objPoints[i] * meshToWorld);
    }

    // Find closest vertex to the closest point.
    double closestVertexDist = -1.0;
    for (unsigned int i = 0; i < numVerts; ++i) {
        const double d = worldPoints[i].distanceTo(closestWorld);
        if (closestVertexDist < 0.0 || d < closestVertexDist) {
            closestVertexDist = d;
            closestVertexId = static_cast<int>(i);
        }
    }

    if (falloff == 1) {
        // Surface falloff: geodesic distance along edges from closest vertex.
        if (closestVertexId >= 0 && distance >= 0.0) {
            struct Node {
                double dist;
                int vid;
            };
            auto cmp = [](const Node& a, const Node& b) { return a.dist > b.dist; };
            std::priority_queue<Node, std::vector<Node>, decltype(cmp)> pq(cmp);

            std::vector<double> dist(numVerts, std::numeric_limits<double>::max());
            std::vector<char> visited(numVerts, 0);

            dist[closestVertexId] = 0.0;
            pq.push({0.0, closestVertexId});

            MItMeshVertex vIt(meshObj, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);

            while (!pq.empty()) {
                Node cur = pq.top();
                pq.pop();
                if (cur.dist > distance)
                    break;
                if (visited[cur.vid])
                    continue;
                visited[cur.vid] = 1;
                outIds.append(cur.vid);

                int prevIndex = 0;
                if (vIt.setIndex(cur.vid, prevIndex) != MS::kSuccess)
                    continue;
                MIntArray neighbors;
                vIt.getConnectedVertices(neighbors);
                for (unsigned int i = 0; i < neighbors.length(); ++i) {
                    const int nb = neighbors[i];
                    if (nb < 0 || static_cast<unsigned int>(nb) >= numVerts)
                        continue;
                    if (visited[nb])
                        continue;
                    const double edgeLen = worldPoints[cur.vid].distanceTo(worldPoints[nb]);
                    const double nd = cur.dist + edgeLen;
                    if (nd <= distance && nd < dist[nb]) {
                        dist[nb] = nd;
                        pq.push({nd, nb});
                    }
                }
            }
        }
    } else {
        // Volume falloff: Euclidean distance from closest point.
        for (unsigned int i = 0; i < numVerts; ++i) {
            const double d = worldPoints[i].distanceTo(closestWorld);
            if (d <= distance) {
                outIds.append(static_cast<int>(i));
            }
        }
    }

    MObject outData = outDataFn.create(outIds, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    data.outputValue(aOutVertexIds).setMObject(outData);
    data.outputValue(aOutCount).setInt(static_cast<int>(outIds.length()));
    data.outputValue(aClosestPoint).set3Double(closestWorld.x, closestWorld.y, closestWorld.z);
    data.outputValue(aClosestVertexId).setInt(closestVertexId);
    data.outputValue(aClosestUV).set2Double(closestU, closestV);
    data.outputValue(aClosestFaceId).setInt(closestFaceId);
    data.setClean(plug);
    return MS::kSuccess;
}

// Plugin boilerplate
MStatus initializePlugin(MObject obj) {
    MStatus status;
    MFnPlugin plugin(obj, "NeighboringVertices", "1.0", "Any");

    status = plugin.registerNode("neighboringVertices",
                                 NeighboringVerticesNode::id,
                                 NeighboringVerticesNode::creator,
                                 NeighboringVerticesNode::initialize);
    if (status != MS::kSuccess)
        status.perror("registerNode neighboringVertices");
    return status;
}

MStatus uninitializePlugin(MObject obj) {
    MFnPlugin plugin(obj);
    MStatus status = plugin.deregisterNode(NeighboringVerticesNode::id);
    if (status != MS::kSuccess)
        status.perror("deregisterNode neighboringVertices");
    return status;
}
