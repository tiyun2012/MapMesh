#include "MatchMeshCreatePinCmd.h"
#include "PinLocatorNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MDagModifier.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnSet.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>

namespace {
const char* kSourceSetFlag = "-ss";
const char* kSourceSetLong = "-sourceSet";
const char* kTargetSetFlag = "-ts";
const char* kTargetSetLong = "-targetSet";
const char* kDefaultSourceSet = "MatchMeshSourceSet";
const char* kDefaultTargetSet = "MatchMeshTargetSet";
}

void* MatchMeshCreatePinCmd::creator() { return new MatchMeshCreatePinCmd(); }

MSyntax MatchMeshCreatePinCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kSourceSetFlag, kSourceSetLong, MSyntax::kString);
    syntax.addFlag(kTargetSetFlag, kTargetSetLong, MSyntax::kString);
    return syntax;
}

MStatus MatchMeshCreatePinCmd::resolveMeshFromSet(const MString& setName, MDagPath& outMesh) const {
    MSelectionList sl;
    if (sl.add(setName) != MS::kSuccess) {
        MGlobal::displayError("MatchMesh: set not found: " + setName);
        return MS::kFailure;
    }
    MObject setObj;
    sl.getDependNode(0, setObj);
    if (!setObj.hasFn(MFn::kSet)) {
        MGlobal::displayError("MatchMesh: not a set: " + setName);
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
        if (path.hasFn(MFn::kTransform))
            path.extendToShape();
        if (path.hasFn(MFn::kMesh)) {
            outMesh = path;
            return MS::kSuccess;
        }
    }

    MGlobal::displayError("MatchMesh: set has no mesh member: " + setName);
    return MS::kFailure;
}

static void addPoint(MPoint& sum, int& count, const MPoint& p) {
    sum.x += p.x;
    sum.y += p.y;
    sum.z += p.z;
    ++count;
}

static MObject transformForPath(const MDagPath& path) {
    MDagPath xformPath = path;
    if (xformPath.hasFn(MFn::kShape) && xformPath.length() > 0) {
        xformPath.pop();
    }
    return xformPath.node();
}

bool MatchMeshCreatePinCmd::accumulateSelectedPointsOnMesh(const MDagPath& meshPath,
                                                           const MSelectionList& sel,
                                                           MPoint& outPoint) const {
    MPoint sum(0.0, 0.0, 0.0);
    int count = 0;
    const MObject meshXformObj = transformForPath(meshPath);

    for (MItSelectionList it(sel); !it.isDone(); it.next()) {
        MDagPath path;
        MObject comp;
        if (it.getDagPath(path, comp) != MS::kSuccess)
            continue;
        if (!path.isValid())
            continue;
        if (comp.isNull())
            continue;
        if (path.hasFn(MFn::kTransform))
            path.extendToShape();
        if (!path.hasFn(MFn::kMesh))
            continue;
        if (transformForPath(path) != meshXformObj)
            continue;

        if (comp.apiType() == MFn::kMeshVertComponent) {
            MItMeshVertex vIt(path, comp);
            for (; !vIt.isDone(); vIt.next())
                addPoint(sum, count, vIt.position(MSpace::kWorld));
        } else if (comp.apiType() == MFn::kMeshEdgeComponent) {
            MItMeshEdge eIt(path, comp);
            for (; !eIt.isDone(); eIt.next())
                addPoint(sum, count, eIt.center(MSpace::kWorld));
        } else if (comp.apiType() == MFn::kMeshPolygonComponent) {
            MItMeshPolygon pIt(path, comp);
            for (; !pIt.isDone(); pIt.next())
                addPoint(sum, count, pIt.center(MSpace::kWorld));
        }
    }

    if (count == 0)
        return false;
    outPoint = MPoint(sum.x / count, sum.y / count, sum.z / count);
    return true;
}

bool MatchMeshCreatePinCmd::accumulateAnySelectedComponent(const MSelectionList& sel,
                                                           MPoint& outPoint) const {
    MPoint sum(0.0, 0.0, 0.0);
    int count = 0;

    for (MItSelectionList it(sel); !it.isDone(); it.next()) {
        MDagPath path;
        MObject comp;
        if (it.getDagPath(path, comp) != MS::kSuccess)
            continue;
        if (!path.isValid())
            continue;
        if (comp.isNull())
            continue;
        if (path.hasFn(MFn::kTransform))
            path.extendToShape();
        if (!path.hasFn(MFn::kMesh))
            continue;

        if (comp.apiType() == MFn::kMeshVertComponent) {
            MItMeshVertex vIt(path, comp);
            for (; !vIt.isDone(); vIt.next())
                addPoint(sum, count, vIt.position(MSpace::kWorld));
        } else if (comp.apiType() == MFn::kMeshEdgeComponent) {
            MItMeshEdge eIt(path, comp);
            for (; !eIt.isDone(); eIt.next())
                addPoint(sum, count, eIt.center(MSpace::kWorld));
        } else if (comp.apiType() == MFn::kMeshPolygonComponent) {
            MItMeshPolygon pIt(path, comp);
            for (; !pIt.isDone(); pIt.next())
                addPoint(sum, count, pIt.center(MSpace::kWorld));
        }
    }

    if (count == 0)
        return false;
    outPoint = MPoint(sum.x / count, sum.y / count, sum.z / count);
    return true;
}

MStatus MatchMeshCreatePinCmd::createPinPairAtPoints(const MDagPath& srcMesh,
                                                     const MDagPath& tgtMesh,
                                                     const MPoint& srcPos,
                                                     const MPoint& tgtPos,
                                                     MObject& outSourcePin,
                                                     MObject& outTargetPin) const {
    MStatus status;
    MDagModifier dagMod;
    MObject srcXform = dagMod.createNode("transform", MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    outSourcePin = dagMod.createNode("MatchMeshPin", srcXform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MObject tgtXform = dagMod.createNode("transform", MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    outTargetPin = dagMod.createNode("MatchMeshPin", tgtXform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = dagMod.doIt();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MFnDependencyNode fnSrc(outSourcePin);
    MFnDependencyNode fnTgt(outTargetPin);
    fnSrc.findPlug(PinLocatorNode::aPinType, true).setShort(PinLocatorNode::kSource);
    fnTgt.findPlug(PinLocatorNode::aPinType, true).setShort(PinLocatorNode::kTarget);

    MFnTransform fnSrcXform(srcXform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    fnSrcXform.setTranslation(MVector(srcPos), MSpace::kWorld);
    MFnTransform fnTgtXform(tgtXform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    fnTgtXform.setTranslation(MVector(tgtPos), MSpace::kWorld);

    fnSrc.findPlug(PinLocatorNode::aPartnerMatrix, true)
        .setMObject(MFnMatrixData().create(tgtMesh.inclusiveMatrix(), &status));
    fnTgt.findPlug(PinLocatorNode::aPartnerMatrix, true)
        .setMObject(MFnMatrixData().create(srcMesh.inclusiveMatrix(), &status));
    return MS::kSuccess;
}

MStatus MatchMeshCreatePinCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString sourceSet(kDefaultSourceSet);
    MString targetSet(kDefaultTargetSet);
    if (db.isFlagSet(kSourceSetFlag))
        db.getFlagArgument(kSourceSetFlag, 0, sourceSet);
    if (db.isFlagSet(kTargetSetFlag))
        db.getFlagArgument(kTargetSetFlag, 0, targetSet);

    MDagPath srcMesh, tgtMesh;
    status = resolveMeshFromSet(sourceSet, srcMesh);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = resolveMeshFromSet(targetSet, tgtMesh);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    MPoint srcPos, tgtPos;
    bool foundSrc = accumulateSelectedPointsOnMesh(srcMesh, sel, srcPos);
    bool foundTgt = accumulateSelectedPointsOnMesh(tgtMesh, sel, tgtPos);
    if (!foundSrc && !foundTgt) {
        // Try any selected component (even if not on source/target), else origin.
        MPoint anyPos;
        if (accumulateAnySelectedComponent(sel, anyPos)) {
            srcPos = anyPos;
            tgtPos = anyPos;
        } else {
            // No component selection: default to origin for both pins.
            srcPos = MPoint::origin;
            tgtPos = MPoint::origin;
        }
    }
    if (foundSrc && !foundTgt) {
        // Single-component placement: use the same position for both pins.
        tgtPos = srcPos;
    } else if (!foundSrc && foundTgt) {
        // Single-component placement: use the same position for both pins.
        srcPos = tgtPos;
    }

    MObject srcPin, tgtPin;
    status = createPinPairAtPoints(srcMesh, tgtMesh, srcPos, tgtPos, srcPin, tgtPin);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}
