#include "MatchMeshCreatePinCmd.h"
#include "PinLocatorNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MDagModifier.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnSet.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MRichSelection.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>
#include <sstream>

namespace {
const char* kSourceSetFlag = "-ss";
const char* kSourceSetLong = "-sourceSet";
const char* kTargetSetFlag = "-ts";
const char* kTargetSetLong = "-targetSet";
const char* kDefaultSourceSet = "MatchMeshSourceSet";
const char* kDefaultTargetSet = "MatchMeshTargetSet";
const char* kSourcePanelName = "matchMeshSourcePanel";
const char* kTargetPanelName = "matchMeshTargetPanel";
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

static void getSelectionWithRichFallback(MSelectionList& outSel) {
    MRichSelection rich;
    if (MGlobal::getRichSelection(rich, true) == MS::kSuccess) {
        rich.getSelection(outSel);
        if (outSel.length() > 0)
            return;
    }
    MGlobal::getActiveSelectionList(outSel);
}

static bool ensureMeshShapePath(MDagPath& path) {
    if (path.hasFn(MFn::kMesh))
        return true;
    if (!path.hasFn(MFn::kTransform))
        return false;

    MFnDagNode fnXform(path);
    // Prefer a non-intermediate mesh shape to match component selections.
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
    // Fallback: accept any mesh child if only intermediate shapes exist.
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

static void addToViewSelectedSet(const MDagPath& xformPath, const char* panelName) {
    if (!xformPath.isValid() || !panelName)
        return;

    MString setName(panelName);
    setName += "ViewSelectedSet";

    MSelectionList sl;
    if (sl.add(setName) != MS::kSuccess)
        return;

    MObject setObj;
    sl.getDependNode(0, setObj);
    if (!setObj.hasFn(MFn::kSet))
        return;

    MStatus status;
    MFnSet fnSet(setObj, &status);
    if (status != MS::kSuccess)
        return;

    fnSet.addMember(xformPath);
}

static void renamePinNodes(const MDagPath& xformPath, const MObject& shapeObj, const char* prefix) {
    if (!prefix)
        return;

    MStatus status;
    MString base(prefix);
    base += "Pin";

    MFnDagNode fnXform(xformPath, &status);
    if (status == MS::kSuccess) {
        fnXform.setName(base);
    }

    MFnDependencyNode fnShape(shapeObj, &status);
    if (status == MS::kSuccess) {
        MString shapeName(base);
        shapeName += "Shape";
        fnShape.setName(shapeName);
    }
}

static MStatus setTransformTranslation(const MDagPath& xformPath, const MPoint& pos) {
    MStatus status;
    MFnTransform fnXform(xformPath, &status);
    if (status != MS::kSuccess)
        return status;

    status = fnXform.setTranslation(MVector(pos), MSpace::kWorld);
    if (status != MS::kSuccess) {
        // Fallback: set in local space if world-space fails.
        status = fnXform.setTranslation(MVector(pos), MSpace::kTransform);
    }
    if (status != MS::kSuccess) {
        // Last resort: write translate plugs directly (assumes world parent).
        MStatus plugStatus;
        MPlug tPlug = fnXform.findPlug("translate", true, &plugStatus);
        if (plugStatus == MS::kSuccess && tPlug.numChildren() >= 3) {
            tPlug.child(0).setDouble(pos.x);
            tPlug.child(1).setDouble(pos.y);
            tPlug.child(2).setDouble(pos.z);
            status = MS::kSuccess;
        }
    }

    return status;
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
        if (!ensureMeshShapePath(path))
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
        if (!ensureMeshShapePath(path))
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
    // Create the pin shapes directly; Maya will create parent transforms.
    outSourcePin = dagMod.createNode("MatchMeshPin", MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    outTargetPin = dagMod.createNode("MatchMeshPin", MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = dagMod.doIt();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDagPath srcPath;
    status = MDagPath::getAPathTo(outSourcePin, srcPath);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDagPath tgtPath;
    status = MDagPath::getAPathTo(outTargetPin, tgtPath);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDagPath srcShapePath = srcPath;
    if (srcShapePath.hasFn(MFn::kTransform))
        srcShapePath.extendToShape();
    MDagPath tgtShapePath = tgtPath;
    if (tgtShapePath.hasFn(MFn::kTransform))
        tgtShapePath.extendToShape();

    MDagPath srcXformPath = srcPath;
    if (!srcXformPath.hasFn(MFn::kTransform)) {
        if (srcXformPath.length() > 0)
            srcXformPath.pop();
    }
    MDagPath tgtXformPath = tgtPath;
    if (!tgtXformPath.hasFn(MFn::kTransform)) {
        if (tgtXformPath.length() > 0)
            tgtXformPath.pop();
    }

    if (!srcXformPath.isValid() || !srcXformPath.hasFn(MFn::kTransform)) {
        MGlobal::displayError("MatchMeshCreatePin: source transform path invalid.");
        return MS::kFailure;
    }
    if (!tgtXformPath.isValid() || !tgtXformPath.hasFn(MFn::kTransform)) {
        MGlobal::displayError("MatchMeshCreatePin: target transform path invalid.");
        return MS::kFailure;
    }

    // Rename transforms and shapes with source/target prefixes for easier identification.
    renamePinNodes(srcXformPath, srcShapePath.node(), "source");
    renamePinNodes(tgtXformPath, tgtShapePath.node(), "target");

    MFnDependencyNode fnSrc(srcShapePath.node());
    MFnDependencyNode fnTgt(tgtShapePath.node());
    fnSrc.findPlug(PinLocatorNode::aPinType, true).setShort(PinLocatorNode::kSource);
    fnTgt.findPlug(PinLocatorNode::aPinType, true).setShort(PinLocatorNode::kTarget);

    status = setTransformTranslation(srcXformPath, srcPos);
    if (status != MS::kSuccess) {
        MGlobal::displayError("MatchMeshCreatePin: failed to set source transform translation.");
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    status = setTransformTranslation(tgtXformPath, tgtPos);
    if (status != MS::kSuccess) {
        MGlobal::displayError("MatchMeshCreatePin: failed to set target transform translation.");
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    // If isolate-select is active, add pins to panel view-selected sets so they stay visible.
    addToViewSelectedSet(srcXformPath, kSourcePanelName);
    addToViewSelectedSet(tgtXformPath, kTargetPanelName);

    {
        MFnTransform fnSrcXform(srcXformPath);
        MFnTransform fnTgtXform(tgtXformPath);
        const MVector actualSrc = fnSrcXform.translation(MSpace::kWorld);
        const MVector actualTgt = fnTgtXform.translation(MSpace::kWorld);
        std::ostringstream oss;
        oss << "MatchMeshCreatePin: set src=("
            << actualSrc.x << ", " << actualSrc.y << ", " << actualSrc.z
            << ") tgt=("
            << actualTgt.x << ", " << actualTgt.y << ", " << actualTgt.z << ")";
        MGlobal::displayInfo(oss.str().c_str());
    }

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
    getSelectionWithRichFallback(sel);
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
            if (sel.length() > 0) {
                MGlobal::displayWarning(
                    "MatchMesh: selection has no mesh components (verts/edges/faces).");
            }
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

    {
        std::ostringstream oss;
        oss << "MatchMeshCreatePin: foundSrc=" << foundSrc
            << " foundTgt=" << foundTgt
            << " srcPos=(" << srcPos.x << ", " << srcPos.y << ", " << srcPos.z
            << ") tgtPos=(" << tgtPos.x << ", " << tgtPos.y << ", " << tgtPos.z << ")";
        MGlobal::displayInfo(oss.str().c_str());
    }

    MObject srcPin, tgtPin;
    status = createPinPairAtPoints(srcMesh, tgtMesh, srcPos, tgtPos, srcPin, tgtPin);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}
