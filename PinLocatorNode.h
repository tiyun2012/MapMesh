#pragma once

#include <maya/MPxLocatorNode.h>
#include <maya/MObject.h>
#include <maya/MTypeId.h>
#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MColor.h>
#include <maya/MDrawRegistry.h>
#include <maya/MUserData.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MBoundingBox.h>
#include <maya/M3dView.h>
#include <maya/MSelectionMask.h>

class PinLocatorNode : public MPxLocatorNode {
public:
    PinLocatorNode() = default;
    ~PinLocatorNode() override = default;

    static void* creator();
    static MStatus initialize();

    void draw(M3dView& view,
              const MDagPath& path,
              M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;

    MSelectionMask getShapeSelectionMask() const override {
        return MSelectionMask::kSelectLocators;
    }

    static MTypeId id;
    static MString drawDbClassification;
    static MString drawRegistrantId;

    static MObject aActive;
    static MObject aRadius;
    static MObject aPinType;
    static MObject aPartnerMatrix;

    enum PinType {
        kSource = 0,
        kTarget = 1
    };
};

// Viewport 2.0 draw override
class PinDrawOverride : public MHWRender::MPxDrawOverride {
public:
    static MHWRender::MPxDrawOverride* creator(const MObject& obj) {
        return new PinDrawOverride(obj);
    }

    MHWRender::DrawAPI supportedDrawAPIs() const override {
        return MHWRender::kAllDevices;
    }

    bool hasUIDrawables() const override { return true; }

    bool isBounded(const MDagPath&,
                   const MDagPath&) const override { return true; }

    MBoundingBox boundingBox(const MDagPath&,
                             const MDagPath&) const override;

    MUserData* prepareForDraw(const MDagPath& objPath,
                              const MDagPath& cameraPath,
                              const MHWRender::MFrameContext& frameContext,
                              MUserData* oldData) override;

    void addUIDrawables(const MDagPath& objPath,
                        MHWRender::MUIDrawManager& drawManager,
                        const MHWRender::MFrameContext& frameContext,
                        const MUserData* data) override;

private:
    explicit PinDrawOverride(const MObject& obj);
    MObject fObject;
};
