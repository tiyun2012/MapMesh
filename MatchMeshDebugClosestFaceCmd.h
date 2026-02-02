#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class MatchMeshDebugClosestFaceCmd : public MPxCommand {
public:
    MatchMeshDebugClosestFaceCmd() = default;
    ~MatchMeshDebugClosestFaceCmd() override = default;

    static void* creator();
    static MSyntax newSyntax();

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }
};
