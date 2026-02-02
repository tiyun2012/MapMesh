#include "MatchMeshMoveFacesCmd.h"
#include "PinLocatorNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <queue>
#include <unordered_set>
#include <sstream>

namespace {
const char* kMeshFlag = "-m";
const char* kMeshLong = "-mesh";
const char* kSourceSetFlag = "-ss";
const char* kSourceSetLong = "-sourceSet";
const char* kRadiusFlag = "-r";
const char* kRadiusLong = "-radius";
const char* kMaxDepthFlag = "-md";
const char* kMaxDepthLong = "-maxDepth";
const char* kStepFlag = "-s";
const char* kStepLong = "-step";
const char* kNoSelectFlag = "-ns";
const char* kNoSelectLong = "-noSelect";

const char* kDefaultSourceSet = "MatchMeshSourceSet";

static bool ensureMeshShapePath(MDagPath& path) {
    if (path.hasFn(MFn::kMesh))
        return true;
    if (!path.hasFn(MFn::kTransform))
        return false;

    MFnDagNode fnXform(path);
    for (unsigned int i = 0; i < fnXform.childCount(); ++i) {
        MObject child = fnXform.child(i);
        if (!child.hasFn(MFn::kMesh))
            continue;
        MFnDagNode fnChild(child);
        if (fnChild.isIntermediateObject())
            continue;
        MDagPath childPath = path;
        childPath.push(child);
        path = childPath;
        return true;
    }
    for (unsigned int i = 0; i < fnXform.childCount(); ++i) {
        MObject child = fnXform.child(i);
        if (!child.hasFn(MFn::kMesh))
            continue;
        MDagPath childPath = path;
        childPath.push(child);
        path = childPath;
        return true;
    }
    return false;
}

static MStatus resolveMeshFromSet(const MString& setName, MDagPath& outMesh) {
    MSelectionList sl;
    if (sl.add(setName) != MS::kSuccess) {
        MGlobal::displayError("MatchMeshMoveFaces: set not found: " + setName);
        return MS::kFailure;
    }
    MObject setObj;
    sl.getDependNode(0, setObj);
    if (!setObj.hasFn(MFn::kSet)) {
        MGlobal::displayError("MatchMeshMoveFaces: not a set: " + setName);
        return MS::kFailure;
    }

    MFnSet fnSet(setObj);
    MSelectionList members;
    fnSet.getMembers(members, true);
    for (MItSelectionList it(members, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath path;
        it.getDagPath(path);
        if (!path.isValid())
            continue;
        if (ensureMeshShapePath(path)) {
            outMesh = path;
            return MS::kSuccess;
        }
    }

    MGlobal::displayError("MatchMeshMoveFaces: set has no mesh member: " + setName);
    return MS::kFailure;
}

static bool isPinShapeObject(const MObject& obj) {
    if (!obj.hasFn(MFn::kPluginLocatorNode) && !obj.hasFn(MFn::kLocator))
        return false;
    MFnDependencyNode fn(obj);
    return fn.typeId() == PinLocatorNode::id;
}

static bool resolvePinShapeFromPath(const MDagPath& path, MDagPath& outShape) {
    if (!path.isValid())
        return false;

    if (isPinShapeObject(path.node())) {
        outShape = path;
        return true;
    }

    if (path.hasFn(MFn::kTransform)) {
        MFnDagNode fn(path);
        for (unsigned int i = 0; i < fn.childCount(); ++i) {
            MObject child = fn.child(i);
            if (!isPinShapeObject(child))
                continue;
            MDagPath childPath = path;
            childPath.push(child);
            outShape = childPath;
            return true;
        }
    }

    return false;
}

static void collectSelectedSourcePins(MSelectionList& outPins) {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    for (MItSelectionList it(sel, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath path;
        it.getDagPath(path);
        MDagPath shapePath;
        if (!resolvePinShapeFromPath(path, shapePath))
            continue;
        MPlug typePlug(shapePath.node(), PinLocatorNode::aPinType);
        short pinType = 0;
        typePlug.getValue(pinType);
        if (pinType == PinLocatorNode::kSource) {
            outPins.add(shapePath);
        }
    }
}

static bool isFaceWithinRadius(const MFnMesh& fnMesh,
                               int faceId,
                               const MPoint& center,
                               double radius) {
    MIntArray verts;
    if (fnMesh.getPolygonVertices(faceId, verts) != MS::kSuccess || verts.length() == 0)
        return false;

    for (unsigned int i = 0; i < verts.length(); ++i) {
        MPoint p;
        if (fnMesh.getPoint(verts[i], p, MSpace::kWorld) != MS::kSuccess)
            return false;
        if (center.distanceTo(p) <= radius)
            return true;
    }
    return false;
}

static void bfsCollectFacesWithinRadius(const MDagPath& meshPath,
                                        const MFnMesh& fnMesh,
                                        int startFaceId,
                                        const MPoint& center,
                                        double radius,
                                        int maxDepth,
                                        MIntArray& outFaces) {
    std::queue<std::pair<int, int>> q;
    std::unordered_set<int> visited;

    q.push(std::make_pair(startFaceId, 0));
    visited.insert(startFaceId);

    MStatus status;
    MItMeshPolygon polyIt(meshPath.node(), &status);
    if (status != MS::kSuccess)
        return;

    while (!q.empty()) {
        const int faceId = q.front().first;
        const int depth = q.front().second;
        q.pop();

        if (isFaceWithinRadius(fnMesh, faceId, center, radius))
            outFaces.append(faceId);

        if (depth >= maxDepth)
            continue;

        int prevIndex = 0;
        if (polyIt.setIndex(faceId, prevIndex) != MS::kSuccess)
            continue;
        MIntArray neighbors;
        polyIt.getConnectedFaces(neighbors);
        for (unsigned int i = 0; i < neighbors.length(); ++i) {
            const int nb = neighbors[i];
            if (visited.insert(nb).second) {
                q.push(std::make_pair(nb, depth + 1));
            }
        }
    }
}
}

void* MatchMeshMoveFacesCmd::creator() { return new MatchMeshMoveFacesCmd(); }

MSyntax MatchMeshMoveFacesCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kMeshFlag, kMeshLong, MSyntax::kString);
    syntax.addFlag(kSourceSetFlag, kSourceSetLong, MSyntax::kString);
    syntax.addFlag(kRadiusFlag, kRadiusLong, MSyntax::kDouble);
    syntax.addFlag(kMaxDepthFlag, kMaxDepthLong, MSyntax::kLong);
    syntax.addFlag(kStepFlag, kStepLong, MSyntax::kDouble);
    syntax.addFlag(kNoSelectFlag, kNoSelectLong);
    return syntax;
}

MStatus MatchMeshMoveFacesCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString meshName;
    MString sourceSet(kDefaultSourceSet);
    if (db.isFlagSet(kMeshFlag)) db.getFlagArgument(kMeshFlag, 0, meshName);
    if (db.isFlagSet(kSourceSetFlag)) db.getFlagArgument(kSourceSetFlag, 0, sourceSet);

    double radius = 0.0;
    int maxDepth = 50;
    const bool hasStep = db.isFlagSet(kStepFlag);
    double step = 0.0;
    if (db.isFlagSet(kRadiusFlag)) db.getFlagArgument(kRadiusFlag, 0, radius);
    if (db.isFlagSet(kMaxDepthFlag)) db.getFlagArgument(kMaxDepthFlag, 0, maxDepth);
    if (hasStep) db.getFlagArgument(kStepFlag, 0, step);

    const bool noSelect = db.isFlagSet(kNoSelectFlag);

    MDagPath meshPath;
    if (meshName.length() > 0) {
        MSelectionList sl;
        if (sl.add(meshName) != MS::kSuccess) {
            MGlobal::displayError("MatchMeshMoveFaces: mesh not found: " + meshName);
            return MS::kFailure;
        }
        sl.getDagPath(0, meshPath);
        if (!ensureMeshShapePath(meshPath)) {
            MGlobal::displayError("MatchMeshMoveFaces: node is not a mesh: " + meshName);
            return MS::kFailure;
        }
    } else {
        status = resolveMeshFromSet(sourceSet, meshPath);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    MSelectionList pins;
    collectSelectedSourcePins(pins);
    if (pins.length() == 0) {
        MGlobal::displayError("MatchMeshMoveFaces: select one or more source pins.");
        return MS::kFailure;
    }

    MFnMesh fnMesh(meshPath, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    std::unordered_set<int> movedFaces;
    std::unordered_set<int> movedVerts;
    int pinCount = 0;

    for (MItSelectionList it(pins, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath pinShapePath;
        it.getDagPath(pinShapePath);
        if (!pinShapePath.isValid())
            continue;

        MDagPath pinXformPath = pinShapePath;
        if (pinXformPath.hasFn(MFn::kShape) && pinXformPath.length() > 0)
            pinXformPath.pop();
        if (!pinXformPath.hasFn(MFn::kTransform))
            continue;

        MFnTransform fnPinXform(pinXformPath, &status);
        if (status != MS::kSuccess)
            continue;
        const MVector pinVec = fnPinXform.translation(MSpace::kWorld);
        const MPoint pinPos(pinVec);

        MFnDependencyNode fnPin(pinShapePath.node());
        MPlug mvPlug = fnPin.findPlug(PinLocatorNode::aMoveVector, true);
        if (mvPlug.numChildren() < 3)
            continue;
        MVector moveVec(mvPlug.child(0).asDouble(),
                        mvPlug.child(1).asDouble(),
                        mvPlug.child(2).asDouble());
        const double moveLen = moveVec.length();
        if (moveLen < 1e-8)
            continue;

        if (hasStep) {
            moveVec.normalize();
            moveVec *= step;
        }

        MPoint closestPoint;
        int faceId = -1;
        status = fnMesh.getClosestPoint(pinPos, closestPoint, MSpace::kWorld, &faceId);
        if (status != MS::kSuccess || faceId < 0)
            continue;

        MIntArray faces;
        if (radius > 0.0) {
            bfsCollectFacesWithinRadius(meshPath, fnMesh, faceId, closestPoint, radius, maxDepth, faces);
        } else {
            faces.append(faceId);
        }

        for (unsigned int i = 0; i < faces.length(); ++i)
            movedFaces.insert(faces[i]);

        for (unsigned int i = 0; i < faces.length(); ++i) {
            MIntArray verts;
            if (fnMesh.getPolygonVertices(faces[i], verts) != MS::kSuccess)
                continue;
            for (unsigned int v = 0; v < verts.length(); ++v)
                movedVerts.insert(verts[v]);
        }

        pinCount++;
    }

    if (movedVerts.empty()) {
        MGlobal::displayWarning("MatchMeshMoveFaces: no vertices found to move.");
        return MS::kSuccess;
    }

    // Re-apply per-pin moves so each pin affects its own region.
    // This avoids blending vectors from different pins.
    for (MItSelectionList it(pins, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath pinShapePath;
        it.getDagPath(pinShapePath);
        if (!pinShapePath.isValid())
            continue;

        MDagPath pinXformPath = pinShapePath;
        if (pinXformPath.hasFn(MFn::kShape) && pinXformPath.length() > 0)
            pinXformPath.pop();
        if (!pinXformPath.hasFn(MFn::kTransform))
            continue;

        MFnTransform fnPinXform(pinXformPath, &status);
        if (status != MS::kSuccess)
            continue;
        const MVector pinVec = fnPinXform.translation(MSpace::kWorld);
        const MPoint pinPos(pinVec);

        MFnDependencyNode fnPin(pinShapePath.node());
        MPlug mvPlug = fnPin.findPlug(PinLocatorNode::aMoveVector, true);
        if (mvPlug.numChildren() < 3)
            continue;
        MVector moveVec(mvPlug.child(0).asDouble(),
                        mvPlug.child(1).asDouble(),
                        mvPlug.child(2).asDouble());
        const double moveLen = moveVec.length();
        if (moveLen < 1e-8)
            continue;

        if (hasStep) {
            moveVec.normalize();
            moveVec *= step;
        }

        MPoint closestPoint;
        int faceId = -1;
        status = fnMesh.getClosestPoint(pinPos, closestPoint, MSpace::kWorld, &faceId);
        if (status != MS::kSuccess || faceId < 0)
            continue;

        MIntArray faces;
        if (radius > 0.0) {
            bfsCollectFacesWithinRadius(meshPath, fnMesh, faceId, closestPoint, radius, maxDepth, faces);
        } else {
            faces.append(faceId);
        }

        std::unordered_set<int> vertsToMove;
        for (unsigned int i = 0; i < faces.length(); ++i) {
            MIntArray verts;
            if (fnMesh.getPolygonVertices(faces[i], verts) != MS::kSuccess)
                continue;
            for (unsigned int v = 0; v < verts.length(); ++v)
                vertsToMove.insert(verts[v]);
        }

        for (const int vId : vertsToMove) {
            MPoint p;
            if (fnMesh.getPoint(vId, p, MSpace::kWorld) != MS::kSuccess)
                continue;
            p += moveVec;
            fnMesh.setPoint(vId, p, MSpace::kWorld);
        }
    }

    if (!noSelect && !movedFaces.empty()) {
        MFnSingleIndexedComponent compFn;
        MObject compObj = compFn.create(MFn::kMeshPolygonComponent, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        for (const int fId : movedFaces)
            compFn.addElement(fId);
        MSelectionList faceSel;
        faceSel.add(meshPath, compObj);
        MGlobal::setActiveSelectionList(faceSel, MGlobal::kReplaceList);
    }

    std::ostringstream oss;
    oss << "MatchMeshMoveFaces: pins=" << pinCount
        << " faces=" << movedFaces.size()
        << " verts=" << movedVerts.size();
    if (hasStep)
        oss << " step=" << step;
    if (radius > 0.0)
        oss << " radius=" << radius << " maxDepth=" << maxDepth;
    MGlobal::displayInfo(oss.str().c_str());

    return MS::kSuccess;
}
