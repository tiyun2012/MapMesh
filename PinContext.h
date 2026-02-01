#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MPoint.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/M3dView.h>
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MFnDependencyNode.h>

class PinContext : public MPxContext {
public:
    PinContext();
    void toolOnSetup(MEvent& event) override;
    MStatus doPress(MEvent& event) override;
    MStatus doDrag(MEvent& event) override;
    MStatus doRelease(MEvent& event) override;
    MStatus helpStateHasChanged(MEvent& event) override;

private:
    MPoint projectToSurface(const short x, const short y, MDagPath& meshPath) const;
    bool findTargetMesh(MDagPath& outPath) const;
    void updatePin(const MPoint& pos);
    MPoint baryToPoint(int faceId, int triId, double b1, double b2, const MDagPath& meshPath) const;

    MDagPath m_targetMesh;
    MDagPath m_activePin;
    // cached last hit for smoother sliding
    mutable bool m_hasLastHit = false;
    mutable int m_lastFace = -1;
    mutable int m_lastTri = -1;
    mutable double m_lastBary1 = 0.0;
    mutable double m_lastBary2 = 0.0;
    mutable MPoint m_lastPoint;
};

class PinContextCommand : public MPxContextCommand {
public:
    PinContextCommand() = default;
    MPxContext* makeObj() override { return new PinContext(); }
    static void* creator() { return new PinContextCommand(); }
};
