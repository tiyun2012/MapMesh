#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class MatchMeshMoveFacesCmd : public MPxCommand {
public:
    MatchMeshMoveFacesCmd() = default;
    ~MatchMeshMoveFacesCmd() override = default;

    static void* creator();
    static MSyntax newSyntax();

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }
};
