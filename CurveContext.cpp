#include "CurveContext.h"

#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/M3dView.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatVector.h>
#include <maya/MDagModifier.h>
#include <maya/MFnDagNode.h>

CurveContext::CurveContext() {
    setTitleString("MatchMesh Curve Tool");
}

void CurveContext::toolOnSetup(MEvent&) {
    MGlobal::displayInfo("MatchMesh Curve Tool: Click on mesh to start drawing a curve. Press Enter to finish.");
    m_isDrawing = false;
    m_currentCurve = MObject::kNullObj;
}

bool CurveContext::findTargetMesh(MDagPath& outPath) const {
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    MItSelectionList it(sel, MFn::kMesh);
    if (it.isDone()) return false;
    it.getDagPath(outPath);
    outPath.extendToShape();
    return true;
}

MPoint CurveContext::projectToSurface(short x, short y, MDagPath& meshPath) {
    M3dView view = M3dView::active3dView();
    MPoint nearP, farP;
    view.viewToWorld(x, y, nearP, farP);
    MVector dir = farP - nearP;

    MFnMesh fnMesh(meshPath);
    MFloatPoint hit;
    auto accel = fnMesh.autoUniformGridParams();
    bool hitRes = fnMesh.closestIntersection(
        MFloatPoint(nearP), MFloatVector(dir),
        nullptr, nullptr, false, MSpace::kWorld, 99999.0f, false,
        &accel, hit, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (hitRes) return MPoint(hit);
    return nearP;
}

MStatus CurveContext::doPress(MEvent& event) {
    short x, y; event.getPosition(x, y);

    if (!m_isDrawing) {
        if (!findTargetMesh(m_targetMesh)) {
            MGlobal::displayWarning("Select a mesh first to draw on.");
            return MS::kFailure;
        }
        MPoint p = projectToSurface(x, y, m_targetMesh);
        createCurve(p);
        m_isDrawing = true;
    } else {
        MPoint p = projectToSurface(x, y, m_targetMesh);
        appendToCurve(p);
    }
    return MS::kSuccess;
}

MStatus CurveContext::doDrag(MEvent&) { return MS::kSuccess; }
MStatus CurveContext::doRelease(MEvent&) { return MS::kSuccess; }
MStatus CurveContext::doEnterRegion(MEvent&) { return MS::kSuccess; }

MStatus CurveContext::doHold(MEvent&) {
    if (m_isDrawing) {
        completeCurve();
        m_isDrawing = false;
        m_currentCurve = MObject::kNullObj;
        return MS::kSuccess;
    }
    return MS::kFailure;
}

void CurveContext::createCurve(const MPoint& p) {
    MPointArray cvs;
    cvs.append(p);
    cvs.append(p); // duplicate to initialize

    MFnNurbsCurve fn;
    MDoubleArray knots;
    knots.append(0.0); knots.append(1.0);

    MStatus stat;
    m_currentCurve = fn.create(cvs, knots, 1, MFnNurbsCurve::kOpen, false, false, MObject::kNullObj, &stat);
    if (stat) {
        MFnDependencyNode fnNode(m_currentCurve);
        fnNode.setName("MatchMeshCurve#");
        MGlobal::executeCommand("refresh");
    }
}

void CurveContext::appendToCurve(const MPoint& p) {
    if (m_currentCurve.isNull()) return;

    MFnNurbsCurve fnCurve(m_currentCurve);
    MPointArray cvs;
    fnCurve.getCVs(cvs);
    cvs.append(p);

    // rebuild simple linear curve
    MDoubleArray knots;
    int numCVs = cvs.length();
    for (int i = 0; i < numCVs; ++i) knots.append(static_cast<double>(i));

    MObject parent = MObject::kNullObj;
    MFnDagNode fnDag(m_currentCurve);
    if (fnDag.parentCount() > 0) {
        parent = fnDag.parent(0);
    }
    
    MGlobal::deleteNode(m_currentCurve);

    MStatus stat;
    m_currentCurve = fnCurve.create(cvs, knots, 1, MFnNurbsCurve::kOpen, false, false, parent, &stat);
    if (stat) MGlobal::executeCommand("refresh");
}

void CurveContext::completeCurve() {
    if (m_currentCurve.isNull()) return;
    // Optional smoothing/degree change could be added here.
    MGlobal::displayInfo("MatchMesh curve completed.");
}
