#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

class MatchMeshCmd : public MPxCommand {
public:
    MatchMeshCmd() = default;
    ~MatchMeshCmd() override = default;

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }

    static void* creator();
    static MSyntax newSyntax();

private:
    MStatus createPinPairAtPoints(const MDagPath& srcMesh,
                                  const MDagPath& tgtMesh,
                                  const MPoint& srcPos,
                                  const MPoint& tgtPos,
                                  MObject& outSourcePin,
                                  MObject& outTargetPin);
};
