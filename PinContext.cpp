#include "PinContext.h"
#include "PinLocatorNode.h"

#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatVector.h>
#include <maya/MMeshIntersector.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MMatrix.h>
#include <maya/M3dView.h>
#include <maya/MFnDependencyNode.h>

PinContext::PinContext() {
    setTitleString("MatchMesh Pin");
    setImage("moveManip.xpm", MPxContext::kImage1);
}

void PinContext::toolOnSetup(MEvent&) {
    MGlobal::displayInfo("MatchMesh Pin Tool: Click a mesh point to move the active pin.");
}

bool PinContext::findTargetMesh(MDagPath& outPath) const {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    MItSelectionList it(sel, MFn::kMesh);
    if (it.isDone()) return false;
    it.getDagPath(outPath);
    outPath.extendToShape();
    return true;
}

MPoint PinContext::projectToSurface(const short x, const short y, MDagPath& meshPath) const {
    M3dView view = M3dView::active3dView();
    MPoint nearP, farP;
    view.viewToWorld(x, y, nearP, farP);
    MVector dir = farP - nearP;
    MFloatPoint hit;
    float hitRayParam = 0.0f;
    int hitFace = -1, hitTriangle = -1;
    float hitBary1 = 0.0f, hitBary2 = 0.0f;
    MFnMesh fnMesh(meshPath);
    auto accelParams = fnMesh.autoUniformGridParams();
    bool hitResult = fnMesh.closestIntersection(
        MFloatPoint(nearP),
        MFloatVector(dir),
        nullptr,
        nullptr,
        false,
        MSpace::kWorld,
        99999.0f,
        false,
        &accelParams,
        hit,
        &hitRayParam,
        &hitFace,
        &hitTriangle,
        &hitBary1,
        &hitBary2);
    if (hitResult) {
        m_hasLastHit = true;
        m_lastFace = hitFace;
        m_lastTri = hitTriangle;
        m_lastBary1 = hitBary1;
        m_lastBary2 = hitBary2;
        m_lastPoint = MPoint(hit);
        return MPoint(hit);
    }
    // fallback: reuse last valid barycentric position on this mesh
    if (m_hasLastHit && meshPath == m_targetMesh) {
        return baryToPoint(m_lastFace, m_lastTri, m_lastBary1, m_lastBary2, meshPath);
    }
    return nearP;
}

MStatus PinContext::doPress(MEvent& event) {
    short x, y;
    event.getPosition(x, y);
    if (!findTargetMesh(m_targetMesh)) {
        MGlobal::displayWarning("Select a mesh first.");
        return MS::kFailure;
    }
    // Assume a pin locator is selected; get first PinLocator in selection.
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    MItSelectionList it(sel, MFn::kLocator);
    for (; !it.isDone(); it.next()) {
        it.getDagPath(m_activePin);
        if (m_activePin.node().hasFn(MFn::kLocator)) break;
    }
    if (!m_activePin.isValid()) {
        MGlobal::displayWarning("No pin locator selected.");
        return MS::kFailure;
    }
    MPoint hit = projectToSurface(x, y, m_targetMesh);
    updatePin(hit);
    return MS::kSuccess;
}

MStatus PinContext::doDrag(MEvent& event) {
    if (!m_activePin.isValid()) return MS::kFailure;
    short x, y; event.getPosition(x, y);
    MPoint hit = projectToSurface(x, y, m_targetMesh);
    updatePin(hit);
    return MS::kSuccess;
}

MStatus PinContext::doRelease(MEvent&) {
    m_activePin = MDagPath();
    m_hasLastHit = false;
    return MS::kSuccess;
}

void PinContext::updatePin(const MPoint& pos) {
    if (!m_activePin.isValid()) return;
    MDagPath xformPath = m_activePin;
    if (xformPath.hasFn(MFn::kTransform) == false && xformPath.length() > 0) {
        xformPath.pop();
    }
    MFnTransform fnPin(xformPath);
    fnPin.setTranslation(MVector(pos), MSpace::kWorld);
}

MStatus PinContext::helpStateHasChanged(MEvent&) {
    return MS::kSuccess;
}

MPoint PinContext::baryToPoint(int faceId, int triId, double b1, double b2, const MDagPath& meshPath) const {
    MFnMesh fnMesh(meshPath);
    int triVerts[3];
    if (fnMesh.getPolygonTriangleVertices(faceId, triId, triVerts) != MS::kSuccess) return m_lastPoint;
    MPoint v0, v1, v2;
    fnMesh.getPoint(triVerts[0], v0, MSpace::kWorld);
    fnMesh.getPoint(triVerts[1], v1, MSpace::kWorld);
    fnMesh.getPoint(triVerts[2], v2, MSpace::kWorld);
    double b0 = 1.0 - b1 - b2;
    return v0 * b0 + v1 * b1 + v2 * b2;
}
