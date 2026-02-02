#include "PinLocatorNode.h"

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
#include <maya/MObjectHandle.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MGLFunctionTable.h>

MTypeId PinLocatorNode::id(0x00126b02);
MString PinLocatorNode::drawDbClassification("drawdb/geometry/MatchMeshPin");
MString PinLocatorNode::drawRegistrantId("MatchMeshPinRegistrant");
MObject PinLocatorNode::aActive;
MObject PinLocatorNode::aRadius;
MObject PinLocatorNode::aPinType;
MObject PinLocatorNode::aPartnerMatrix;

void* PinLocatorNode::creator() { return new PinLocatorNode(); }

MStatus PinLocatorNode::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnEnumAttribute eAttr;
    MFnMatrixAttribute mAttr;

    aActive = nAttr.create("active", "act", MFnNumericData::kBoolean, true, &stat);
    nAttr.setKeyable(true);

    aRadius = nAttr.create("radius", "rad", MFnNumericData::kFloat, 0.3f, &stat);
    nAttr.setMin(0.01f);
    nAttr.setKeyable(true);

    aPinType = eAttr.create("pinType", "pt", 0, &stat);
    eAttr.addField("Source", kSource);
    eAttr.addField("Target", kTarget);
    eAttr.setKeyable(true);

    aPartnerMatrix = mAttr.create("partnerMatrix", "pmat", MFnMatrixAttribute::kDouble, &stat);
    mAttr.setStorable(true);
    mAttr.setWritable(true);

    addAttribute(aActive);
    addAttribute(aRadius);
    addAttribute(aPinType);
    addAttribute(aPartnerMatrix);

    return MS::kSuccess;
}

void PinLocatorNode::draw(M3dView& view,
                          const MDagPath& path,
                          M3dView::DisplayStyle style,
                          M3dView::DisplayStatus status) {
    // Legacy viewport fallback (simple cross).
    (void)style;
    (void)status;

    MStatus s;
    float radius = 1.0f;
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

// ---------------- Draw Override ----------------
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
    float radius = 1.0f;
    if (status == MS::kSuccess) {
        MPlug rPlug(objPath.node(), PinLocatorNode::aRadius);
        rPlug.getValue(radius);
    }
    const double r = radius;
    return MBoundingBox(MPoint(-r, -r, -r), MPoint(r, r, r));
}

MUserData* PinDrawOverride::prepareForDraw(const MDagPath& objPath,
                                           const MDagPath&,
                                           const MHWRender::MFrameContext&,
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
                                     const MHWRender::MFrameContext&,
                                     const MUserData* userData) {
    const PinUserData* data = dynamic_cast<const PinUserData*>(userData);
    if (!data) return;
    const unsigned int pickId = MObjectHandle(objPath.node()).hashCode();
    dm.beginDrawable(MHWRender::MUIDrawManager::kSelectable, pickId);
    dm.setColor(data->highlight ? brightenColor(data->color) : data->color);
    // Draw a locator-style cross (better for selection/readability than a sphere).
    const double r = data->radius;
    dm.line(MPoint(-r, 0.0, 0.0), MPoint(r, 0.0, 0.0));
    dm.line(MPoint(0.0, -r, 0.0), MPoint(0.0, r, 0.0));
    dm.line(MPoint(0.0, 0.0, -r), MPoint(0.0, 0.0, r));
    dm.endDrawable();
}
