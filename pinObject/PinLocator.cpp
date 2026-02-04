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

    return MS::kSuccess;
}

MBoundingBox PinLocatorNode::boundingBox() const {
    float radius = 0.3f;
    MPlug rPlug(thisMObject(), PinLocatorNode::aRadius);
    rPlug.getValue(radius);
    const double r = radius;
    return MBoundingBox(MPoint(-r, -r, -r), MPoint(r, r, r));
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
    glFT->glBegin(MGL_LINES);
    glFT->glVertex3f(-radius, 0.0f, 0.0f); glFT->glVertex3f(radius, 0.0f, 0.0f);
    glFT->glVertex3f(0.0f, -radius, 0.0f); glFT->glVertex3f(0.0f, radius, 0.0f);
    glFT->glVertex3f(0.0f, 0.0f, -radius); glFT->glVertex3f(0.0f, 0.0f, radius);
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
    if (status == MS::kSuccess) {
        MPlug rPlug(objPath.node(), PinLocatorNode::aRadius);
        rPlug.getValue(radius);
    }
    const double r = radius;
    return MBoundingBox(MPoint(-r, -r, -r), MPoint(r, r, r));
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
    dm.line(MPoint(-r, 0.0, 0.0), MPoint(r, 0.0, 0.0));
    dm.line(MPoint(0.0, -r, 0.0), MPoint(0.0, r, 0.0));
    dm.line(MPoint(0.0, 0.0, -r), MPoint(0.0, 0.0, r));
    
    dm.endDrawable();
}
