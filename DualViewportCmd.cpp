#include "DualViewportCmd.h"

#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MArgDatabase.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MFn.h>
#include <maya/MString.h>
#include <maya/MDagPathArray.h>
#include <maya/MMatrix.h>
#include <unordered_map>

namespace {
const char* kLeftFlag = "-l";
const char* kLeftLong = "-leftPanel";
const char* kRightFlag = "-r";
const char* kRightLong = "-rightPanel";
std::unordered_map<std::string, MMatrix> gCameraOffsets;
}

void* DualViewportCmd::creator() { return new DualViewportCmd(); }

MSyntax DualViewportCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kLeftFlag, kLeftLong, MSyntax::kString);
    syntax.addFlag(kRightFlag, kRightLong, MSyntax::kString);
    return syntax;
}

MStatus DualViewportCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString leftPanel, rightPanel;
    if (db.isFlagSet(kLeftFlag)) db.getFlagArgument(kLeftFlag, 0, leftPanel);
    if (db.isFlagSet(kRightFlag)) db.getFlagArgument(kRightFlag, 0, rightPanel);

    if (leftPanel.length() == 0 || rightPanel.length() == 0) {
        MGlobal::displayError("Usage: matchMeshSyncView -l leftModelPanel -r rightModelPanel");
        return MS::kFailure;
    }

    M3dView leftView, rightView;
    if (M3dView::getM3dViewFromModelPanel(leftPanel, leftView) != MS::kSuccess) {
        MGlobal::displayError("Left panel not found.");
        return MS::kFailure;
    }
    if (M3dView::getM3dViewFromModelPanel(rightPanel, rightView) != MS::kSuccess) {
        MGlobal::displayError("Right panel not found.");
        return MS::kFailure;
    }
    MDagPath leftCam, rightCam;
    leftView.getCamera(leftCam);
    rightView.getCamera(rightCam);

    // M3dView::getCamera may return the camera SHAPE; we need the transform for MFnTransform.
    if (leftCam.hasFn(MFn::kCamera)) leftCam.pop();
    if (rightCam.hasFn(MFn::kCamera)) rightCam.pop();

    MStatus s;
    MFnTransform leftXform(leftCam, &s);
    if (s != MS::kSuccess) { MGlobal::displayError("Left camera transform invalid."); return MS::kFailure; }
    MFnTransform rightXform(rightCam, &s);
    if (s != MS::kSuccess) { MGlobal::displayError("Right camera transform invalid."); return MS::kFailure; }

    // Work in world space using inclusive/exclusive matrices so parented cameras behave correctly.
    const MMatrix leftWorld = leftCam.inclusiveMatrix();
    const MMatrix rightWorld = rightCam.inclusiveMatrix();

    const std::string key = std::string(leftPanel.asChar()) + "->" + std::string(rightPanel.asChar()) +
                            "|" + std::string(leftCam.fullPathName().asChar()) +
                            "->" + std::string(rightCam.fullPathName().asChar());
    MMatrix offset = MMatrix::identity;
    auto it = gCameraOffsets.find(key);
    if (it == gCameraOffsets.end()) {
        offset = rightWorld * leftWorld.inverse();
        gCameraOffsets[key] = offset;
    } else {
        offset = it->second;
    }

    // Keep the initial relative offset so that right follows left.
    const MMatrix desiredRightWorld = offset * leftWorld;

    // Convert desired world matrix to local (parent) space for setting on the transform.
    const MMatrix desiredRightLocal = desiredRightWorld * rightCam.exclusiveMatrixInverse();
    rightXform.set(MTransformationMatrix(desiredRightLocal));

    // Refresh both panels
    leftView.refresh(true, true);
    rightView.refresh(true, true);
    return MS::kSuccess;
}
