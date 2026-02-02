#include "MatchMeshDebugClosestFaceCmd.h"
#include "PinLocatorNode.h"

#include <maya/MArgDatabase.h>
#include <maya/MDagModifier.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <sstream>

namespace {
const char* kPinFlag = "-p";
const char* kPinLong = "-pin";
const char* kMeshFlag = "-m";
const char* kMeshLong = "-mesh";
const char* kSourceSetFlag = "-ss";
const char* kSourceSetLong = "-sourceSet";
const char* kTargetSetFlag = "-ts";
const char* kTargetSetLong = "-targetSet";
const char* kClearFlag = "-cl";
const char* kClearLong = "-clear";
const char* kNoSelectFlag = "-ns";
const char* kNoSelectLong = "-noSelect";
const char* kNoLocatorFlag = "-nl";
const char* kNoLocatorLong = "-noLocator";

const char* kDefaultSourceSet = "MatchMeshSourceSet";
const char* kDefaultTargetSet = "MatchMeshTargetSet";
const char* kDebugLocatorBase = "matchMeshDebugHit";

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

static bool findPinFromSelection(MDagPath& outShape) {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    for (MItSelectionList it(sel, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath path;
        it.getDagPath(path);
        if (resolvePinShapeFromPath(path, outShape))
            return true;
    }
    return false;
}

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

static bool findMeshFromSelection(MDagPath& outMesh) {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    for (MItSelectionList it(sel, MFn::kDagNode); !it.isDone(); it.next()) {
        MDagPath path;
        it.getDagPath(path);
        if (!path.isValid())
            continue;
        if (ensureMeshShapePath(path)) {
            outMesh = path;
            return true;
        }
    }
    return false;
}

static MStatus resolveMeshFromSet(const MString& setName, MDagPath& outMesh) {
    MSelectionList sl;
    if (sl.add(setName) != MS::kSuccess) {
        MGlobal::displayError("MatchMeshDebug: set not found: " + setName);
        return MS::kFailure;
    }
    MObject setObj;
    sl.getDependNode(0, setObj);
    if (!setObj.hasFn(MFn::kSet)) {
        MGlobal::displayError("MatchMeshDebug: not a set: " + setName);
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

    MGlobal::displayError("MatchMeshDebug: set has no mesh member: " + setName);
    return MS::kFailure;
}

static void clearDebugLocators() {
    MString cmd = "string $mmDbg[] = `ls -type transform \"";
    cmd += kDebugLocatorBase;
    cmd += "*\"`; if (size($mmDbg)) delete $mmDbg;";
    MGlobal::executeCommand(cmd, false, true);
}

static MStatus createDebugLocator(const MPoint& pos, MString& outName) {
    MStatus status;
    MDagModifier dagMod;
    MObject xform = dagMod.createNode("transform", MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MObject shape = dagMod.createNode("locator", xform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = dagMod.doIt();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDagPath xformPath;
    status = MDagPath::getAPathTo(xform, xformPath);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MFnTransform fnXform(xformPath, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = fnXform.setTranslation(MVector(pos), MSpace::kWorld);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MFnDagNode fnXformDag(xform, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MString baseName = fnXformDag.setName(kDebugLocatorBase);
    outName = baseName;

    MFnDagNode fnShape(shape, &status);
    if (status == MS::kSuccess) {
        MString shapeName = baseName + "Shape";
        fnShape.setName(shapeName);

        // Bright yellow override for visibility.
        MStatus plugStatus;
        MPlug ovEn = fnShape.findPlug("overrideEnabled", true, &plugStatus);
        if (plugStatus == MS::kSuccess) ovEn.setBool(true);
        MPlug ovRgb = fnShape.findPlug("overrideRGBColors", true, &plugStatus);
        if (plugStatus == MS::kSuccess) ovRgb.setBool(true);
        MPlug ovCol = fnShape.findPlug("overrideColorRGB", true, &plugStatus);
        if (plugStatus == MS::kSuccess && ovCol.numChildren() >= 3) {
            ovCol.child(0).setDouble(1.0);
            ovCol.child(1).setDouble(1.0);
            ovCol.child(2).setDouble(0.0);
        }
    }

    return MS::kSuccess;
}
}

void* MatchMeshDebugClosestFaceCmd::creator() { return new MatchMeshDebugClosestFaceCmd(); }

MSyntax MatchMeshDebugClosestFaceCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kPinFlag, kPinLong, MSyntax::kString);
    syntax.addFlag(kMeshFlag, kMeshLong, MSyntax::kString);
    syntax.addFlag(kSourceSetFlag, kSourceSetLong, MSyntax::kString);
    syntax.addFlag(kTargetSetFlag, kTargetSetLong, MSyntax::kString);
    syntax.addFlag(kClearFlag, kClearLong);
    syntax.addFlag(kNoSelectFlag, kNoSelectLong);
    syntax.addFlag(kNoLocatorFlag, kNoLocatorLong);
    return syntax;
}

MStatus MatchMeshDebugClosestFaceCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString pinName;
    MString meshName;
    MString sourceSet(kDefaultSourceSet);
    MString targetSet(kDefaultTargetSet);
    if (db.isFlagSet(kPinFlag)) db.getFlagArgument(kPinFlag, 0, pinName);
    if (db.isFlagSet(kMeshFlag)) db.getFlagArgument(kMeshFlag, 0, meshName);
    if (db.isFlagSet(kSourceSetFlag)) db.getFlagArgument(kSourceSetFlag, 0, sourceSet);
    if (db.isFlagSet(kTargetSetFlag)) db.getFlagArgument(kTargetSetFlag, 0, targetSet);

    const bool clear = db.isFlagSet(kClearFlag);
    const bool noSelect = db.isFlagSet(kNoSelectFlag);
    const bool noLocator = db.isFlagSet(kNoLocatorFlag);

    MDagPath pinShapePath;
    if (pinName.length() > 0) {
        MSelectionList sl;
        if (sl.add(pinName) != MS::kSuccess) {
            MGlobal::displayError("MatchMeshDebug: pin not found: " + pinName);
            return MS::kFailure;
        }
        MDagPath path;
        sl.getDagPath(0, path);
        if (!resolvePinShapeFromPath(path, pinShapePath)) {
            MGlobal::displayError("MatchMeshDebug: node is not a MatchMeshPin: " + pinName);
            return MS::kFailure;
        }
    } else {
        if (!findPinFromSelection(pinShapePath)) {
            MGlobal::displayError("MatchMeshDebug: select a MatchMeshPin to debug.");
            return MS::kFailure;
        }
    }

    // Pin world position.
    MDagPath pinXformPath = pinShapePath;
    if (pinXformPath.hasFn(MFn::kShape) && pinXformPath.length() > 0)
        pinXformPath.pop();
    if (!pinXformPath.hasFn(MFn::kTransform)) {
        MGlobal::displayError("MatchMeshDebug: pin transform not found.");
        return MS::kFailure;
    }
    MFnTransform fnPinXform(pinXformPath, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector pinVec = fnPinXform.translation(MSpace::kWorld);
    const MPoint pinPos(pinVec);

    // Determine mesh to query.
    MDagPath meshPath;
    if (meshName.length() > 0) {
        MSelectionList sl;
        if (sl.add(meshName) != MS::kSuccess) {
            MGlobal::displayError("MatchMeshDebug: mesh not found: " + meshName);
            return MS::kFailure;
        }
        sl.getDagPath(0, meshPath);
        if (!ensureMeshShapePath(meshPath)) {
            MGlobal::displayError("MatchMeshDebug: node is not a mesh: " + meshName);
            return MS::kFailure;
        }
    } else if (!findMeshFromSelection(meshPath)) {
        // Default to source mesh when no mesh is explicitly provided.
        status = resolveMeshFromSet(sourceSet, meshPath);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    MFnMesh fnMesh(meshPath, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MPoint closestPoint;
    int faceId = -1;
    status = fnMesh.getClosestPoint(pinPos, closestPoint, MSpace::kWorld, &faceId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    const double dist = pinPos.distanceTo(closestPoint);

    if (clear)
        clearDebugLocators();

    MString locatorName;
    if (!noLocator) {
        status = createDebugLocator(closestPoint, locatorName);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (!noSelect && faceId >= 0) {
        MFnSingleIndexedComponent compFn;
        MObject compObj = compFn.create(MFn::kMeshPolygonComponent, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        compFn.addElement(faceId);
        MSelectionList faceSel;
        faceSel.add(meshPath, compObj);
        MGlobal::setActiveSelectionList(faceSel, MGlobal::kReplaceList);
    }

    std::ostringstream oss;
    oss << "MatchMeshDebug: pin=" << pinShapePath.fullPathName().asChar()
        << " mesh=" << meshPath.fullPathName().asChar()
        << " face=" << faceId
        << " closest=(" << closestPoint.x << ", " << closestPoint.y << ", " << closestPoint.z << ")"
        << " dist=" << dist;
    if (!noLocator)
        oss << " locator=" << locatorName.asChar();
    MGlobal::displayInfo(oss.str().c_str());

    return MS::kSuccess;
}
