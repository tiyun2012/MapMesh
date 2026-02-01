#include "MatchMeshCmd.h"
#include "PinLocatorNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MDagModifier.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>

namespace {
const char* kCreateFlag = "-c";
const char* kCreateLongFlag = "-createPins";

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

static bool accumulateSelectedPointsOnMesh(const MDagPath& meshPath,
                                           const MSelectionList& sel,
                                           MPoint& outPoint) {
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

static MPoint meshWorldCenter(const MDagPath& meshPath) {
    MFnMesh fnMesh(meshPath);
    MPoint center = fnMesh.boundingBox().center();
    center *= meshPath.inclusiveMatrix();
    return center;
}
}

void* MatchMeshCmd::creator() { return new MatchMeshCmd(); }

MSyntax MatchMeshCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kCreateFlag, kCreateLongFlag, MSyntax::kBoolean);
    return syntax;
}

MStatus MatchMeshCmd::createPinPairAtPoints(const MDagPath& srcMesh,
                                            const MDagPath& tgtMesh,
                                            const MPoint& srcPos,
                                            const MPoint& tgtPos,
                                            MObject& outSourcePin,
                                            MObject& outTargetPin) {
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

    // Mark types
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

    // Partner matrices (storable copy)
    fnSrc.findPlug(PinLocatorNode::aPartnerMatrix, true)
        .setMObject(MFnMatrixData().create(tgtMesh.inclusiveMatrix(), &status));
    fnTgt.findPlug(PinLocatorNode::aPartnerMatrix, true)
        .setMObject(MFnMatrixData().create(srcMesh.inclusiveMatrix(), &status));
    return status;
}

MStatus MatchMeshCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    bool createPins = true;
    if (db.isFlagSet(kCreateFlag))
        db.getFlagArgument(kCreateFlag, 0, createPins);

    // Expect: first selected mesh = source, second mesh = target
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    if (sel.length() < 2) {
        MGlobal::displayError("Select source mesh then target mesh before running matchMesh.");
        return MS::kFailure;
    }
    MDagPath srcMesh, tgtMesh;
    sel.getDagPath(0, srcMesh);
    sel.getDagPath(1, tgtMesh);
    srcMesh.extendToShape(); tgtMesh.extendToShape();

    // Optionally create pin pair (selection-based when available).
    if (createPins) {
        MPoint srcPos, tgtPos;
        bool foundSrc = accumulateSelectedPointsOnMesh(srcMesh, sel, srcPos);
        bool foundTgt = accumulateSelectedPointsOnMesh(tgtMesh, sel, tgtPos);
        if (!foundSrc && !foundTgt) {
            srcPos = meshWorldCenter(srcMesh);
            tgtPos = meshWorldCenter(tgtMesh);
        } else if (foundSrc && !foundTgt) {
            tgtPos = srcPos;
        } else if (!foundSrc && foundTgt) {
            srcPos = tgtPos;
        }

        MObject srcPin, tgtPin;
        status = createPinPairAtPoints(srcMesh, tgtMesh, srcPos, tgtPos, srcPin, tgtPin);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    return MS::kSuccess;
}
