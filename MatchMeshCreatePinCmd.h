#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgList.h>
#include <maya/MStatus.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPoint.h>

class MatchMeshCreatePinCmd : public MPxCommand {
public:
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;

private:
    MStatus resolveMeshFromSet(const MString& setName, MDagPath& outMesh) const;
    bool accumulateSelectedPointsOnMesh(const MDagPath& meshPath,
                                        const MSelectionList& sel,
                                        MPoint& outPoint) const;
    bool accumulateAnySelectedComponent(const MSelectionList& sel,
                                        MPoint& outPoint) const;
    MStatus createPinPairAtPoints(const MDagPath& srcMesh,
                                  const MDagPath& tgtMesh,
                                  const MPoint& srcPos,
                                  const MPoint& tgtPos,
                                  MObject& outSourcePin,
                                  MObject& outTargetPin) const;
};
