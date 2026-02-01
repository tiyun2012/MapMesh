#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MPoint.h>
#include <maya/MDagPath.h>
#include <maya/MPointArray.h>
#include <maya/MObject.h>

class CurveContext : public MPxContext {
public:
    CurveContext();
    void toolOnSetup(MEvent& event) override;
    MStatus doPress(MEvent& event) override;
    MStatus doDrag(MEvent& event) override;
    MStatus doRelease(MEvent& event) override;
    MStatus doEnterRegion(MEvent& event) override;
    MStatus doHold(MEvent& event) override;

private:
    MPoint projectToSurface(short x, short y, MDagPath& meshPath);
    bool findTargetMesh(MDagPath& outPath) const;
    void createCurve(const MPoint& p);
    void appendToCurve(const MPoint& p);
    void completeCurve();

    MDagPath m_targetMesh;
    MObject m_currentCurve;
    bool m_isDrawing = false;
};

class CurveContextCommand : public MPxContextCommand {
public:
    CurveContextCommand() = default;
    MPxContext* makeObj() override { return new CurveContext(); }
    static void* creator() { return new CurveContextCommand(); }
};
