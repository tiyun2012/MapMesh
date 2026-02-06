// PinLocator.cpp - FIXED VERSION
#include "PinLocator.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MDrawRegistry.h>
#include <maya/MColor.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MUserData.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MGLFunctionTable.h>
#include <cmath>
#include <algorithm>

// Initialize static members
MTypeId PinLocatorNode::id(0x0012F2A2);
MString PinLocatorNode::drawDbClassification("drawdb/geometry/PinLocator");
MString PinLocatorNode::drawRegistrantId("PinLocatorRegistrant");

MObject PinLocatorNode::aActive;
MObject PinLocatorNode::aRadius;
MObject PinLocatorNode::aPinType;
MObject PinLocatorNode::aMoveVector;
MObject PinLocatorNode::aPartnerMatrix;
MObject PinLocatorNode::aUV;
MObject PinLocatorNode::aLineScale;

void* PinLocatorNode::creator() { return new PinLocatorNode(); }

MStatus PinLocatorNode::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnEnumAttribute eAttr;
    MFnMatrixAttribute mAttr;

    // Active attribute
    aActive = nAttr.create("active", "act", MFnNumericData::kBoolean, true, &stat);
    if (stat != MS::kSuccess) return stat;
    nAttr.setKeyable(true);
    addAttribute(aActive);

    // Radius attribute
    aRadius = nAttr.create("radius", "rad", MFnNumericData::kFloat, 0.3f, &stat);
    if (stat != MS::kSuccess) return stat;
    nAttr.setMin(0.01f);
    nAttr.setKeyable(true);
    addAttribute(aRadius);

    // Pin Type attribute
    aPinType = eAttr.create("pinType", "pt", 0, &stat);
    if (stat != MS::kSuccess) return stat;
    eAttr.addField("Source", kSource);
    eAttr.addField("Target", kTarget);
    eAttr.setKeyable(true);
    addAttribute(aPinType);

    // Move Vector attribute
    aMoveVector = nAttr.create("moveVector", "mv", MFnNumericData::k3Double, 0.0, &stat);
    if (stat != MS::kSuccess) return stat;
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    addAttribute(aMoveVector);

    // Partner Matrix attribute
    aPartnerMatrix = mAttr.create("partnerMatrix", "pmat", MFnMatrixAttribute::kDouble, &stat);
    if (stat != MS::kSuccess) return stat;
    mAttr.setStorable(true);
    mAttr.setWritable(true);
    addAttribute(aPartnerMatrix);

    // UV attributes - FIXED: Use separate numeric attributes
    // UV attribute (double2)
    aUV = nAttr.create("uv", "uv", MFnNumericData::k2Double, 0.0, &stat);
    if (stat != MS::kSuccess) return stat;
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    addAttribute(aUV);

    // Line length scale (relative to radius). Default 2.0 => line length = 2 * radius.
    aLineScale = nAttr.create("lineScale", "ls", MFnNumericData::kDouble, 2.0, &stat);
    if (stat != MS::kSuccess) return stat;
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setMin(0.0);
    addAttribute(aLineScale);

    return MS::kSuccess;
}

MBoundingBox PinLocatorNode::boundingBox() const {
    float radius = 0.3f;
    MPlug rPlug(thisMObject(), PinLocatorNode::aRadius);
    rPlug.getValue(radius);
    double lineScale = 2.0;
    MPlug lsPlug(thisMObject(), PinLocatorNode::aLineScale);
    if (!lsPlug.isNull())
        lsPlug.getValue(lineScale);
    if (lineScale < 0.0)
        lineScale = 0.0;
    const double r = radius;
    const double lineLen = r * lineScale;
    const double rx = std::max(r, lineLen);
    return MBoundingBox(MPoint(-rx, -r, -r), MPoint(rx, r, r));
}

void PinLocatorNode::draw(M3dView& view,
                          const MDagPath& path,
                          M3dView::DisplayStyle style,
                          M3dView::DisplayStatus status) {
    // Legacy viewport fallback (simple cross).
    (void)style;

    float radius = 0.3f;
    MPlug rPlug(path.node(), PinLocatorNode::aRadius);
    rPlug.getValue(radius);
    double lineScale = 2.0;
    MPlug lsPlug(path.node(), PinLocatorNode::aLineScale);
    if (!lsPlug.isNull())
        lsPlug.getValue(lineScale);
    if (lineScale < 0.0)
        lineScale = 0.0;
    short pinType = 0;
    MPlug tPlug(path.node(), PinLocatorNode::aPinType);
    tPlug.getValue(pinType);

    const MColor srcCol(1.0f, 0.35f, 0.2f);
    const MColor tgtCol(0.2f, 0.9f, 0.35f);
    MColor col = (pinType == PinLocatorNode::kSource) ? srcCol : tgtCol;
    if (status == M3dView::kActive || status == M3dView::kLead || status == M3dView::kHilite) {
        // Brighten when selected in the legacy viewport.
        col.r = col.r + (1.0f - col.r) * 0.5f;
        col.g = col.g + (1.0f - col.g) * 0.5f;
        col.b = col.b + (1.0f - col.b) * 0.5f;
    }

    MHardwareRenderer* renderer = MHardwareRenderer::theRenderer();
    MGLFunctionTable* glFT = renderer ? const_cast<MGLFunctionTable*>(renderer->glFunctionTable()) : nullptr;
    if (!glFT)
        return;

    view.beginGL();
    glFT->glColor3f(col.r, col.g, col.b);

    // Circle in YZ plane, normal along +X.
    const int segments = 32;
    const double r = radius;
    const double lineLen = r * lineScale;
    const double twoPi = 6.28318530717958647692;
    glFT->glBegin(MGL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const double t = twoPi * static_cast<double>(i) / static_cast<double>(segments);
        const float y = static_cast<float>(std::cos(t) * r);
        const float z = static_cast<float>(std::sin(t) * r);
        glFT->glVertex3f(0.0f, y, z);
    }
    glFT->glEnd();

    // Normal line along +X.
    glFT->glBegin(MGL_LINES);
    glFT->glVertex3f(0.0f, 0.0f, 0.0f);
    glFT->glVertex3f(static_cast<float>(lineLen), 0.0f, 0.0f);
    glFT->glEnd();

    view.endGL();
}

// ---------------------------------------------------------------------
// PinDrawOverride implementation
// ---------------------------------------------------------------------

namespace {
struct PinUserData : public MUserData {
    PinUserData() : MUserData(false) {}
    float radius = 1.0f;
    short pinType = 0;
    MColor color;
    bool highlight = false;
};

static MColor brightenColor(const MColor& col) {
    return MColor(
        col.r + (1.0f - col.r) * 0.5f,
        col.g + (1.0f - col.g) * 0.5f,
        col.b + (1.0f - col.b) * 0.5f);
}
}

PinDrawOverride::PinDrawOverride(const MObject& obj)
: MHWRender::MPxDrawOverride(obj, nullptr, false), fObject(obj) {}

MBoundingBox PinDrawOverride::boundingBox(const MDagPath& objPath,
                                          const MDagPath&) const {
    MStatus status;
    MFnDependencyNode fn(objPath.node(), &status);
    float radius = 0.3f;
    double lineScale = 2.0;
    if (status == MS::kSuccess) {
        MPlug rPlug(objPath.node(), PinLocatorNode::aRadius);
        rPlug.getValue(radius);
        MPlug lsPlug(objPath.node(), PinLocatorNode::aLineScale);
        if (!lsPlug.isNull())
            lsPlug.getValue(lineScale);
    }
    if (lineScale < 0.0)
        lineScale = 0.0;
    const double r = radius;
    const double lineLen = r * lineScale;
    const double rx = std::max(r, lineLen);
    return MBoundingBox(MPoint(-rx, -r, -r), MPoint(rx, r, r));
}

MUserData* PinDrawOverride::prepareForDraw(const MDagPath& objPath,
                                           const MDagPath& cameraPath,
                                           const MHWRender::MFrameContext& frameContext,
                                           MUserData* oldData) {
    PinUserData* data = dynamic_cast<PinUserData*>(oldData);
    if (!data) data = new PinUserData();

    MPlug radiusPlug(objPath.node(), PinLocatorNode::aRadius);
    radiusPlug.getValue(data->radius);

    MPlug typePlug(objPath.node(), PinLocatorNode::aPinType);
    typePlug.getValue(data->pinType);

    const MColor srcCol(1.0f, 0.35f, 0.2f);
    const MColor tgtCol(0.2f, 0.9f, 0.35f);
    data->color = (data->pinType == PinLocatorNode::kSource) ? srcCol : tgtCol;
    
    const MHWRender::DisplayStatus drawStatus =
        MHWRender::MGeometryUtilities::displayStatus(objPath);
    data->highlight = (drawStatus == MHWRender::kActive ||
                       drawStatus == MHWRender::kLead ||
                       drawStatus == MHWRender::kHilite);
    return data;
}

void PinDrawOverride::addUIDrawables(const MDagPath& objPath,
                                     MHWRender::MUIDrawManager& dm,
                                     const MHWRender::MFrameContext& frameContext,
                                     const MUserData* userData) {
    const PinUserData* data = dynamic_cast<const PinUserData*>(userData);
    if (!data) return;
    
    dm.beginDrawable(MHWRender::MUIDrawManager::kSelectable);
    dm.setColor(data->highlight ? brightenColor(data->color) : data->color);
    
    const double r = data->radius;
    double lineScale = 2.0;
    MPlug lsPlug(objPath.node(), PinLocatorNode::aLineScale);
    if (!lsPlug.isNull())
        lsPlug.getValue(lineScale);
    if (lineScale < 0.0)
        lineScale = 0.0;
    const double lineLen = r * lineScale;
    // Circle in YZ plane, normal along +X.
    const int segments = 32;
    const double twoPi = 6.28318530717958647692;
    for (int i = 0; i < segments; ++i) {
        const double t0 = twoPi * static_cast<double>(i) / static_cast<double>(segments);
        const double t1 = twoPi * static_cast<double>(i + 1) / static_cast<double>(segments);
        const MPoint p0(0.0, std::cos(t0) * r, std::sin(t0) * r);
        const MPoint p1(0.0, std::cos(t1) * r, std::sin(t1) * r);
        dm.line(p0, p1);
    }

    // Normal line along +X.
    dm.line(MPoint(0.0, 0.0, 0.0), MPoint(lineLen, 0.0, 0.0));
    
    dm.endDrawable();
}
