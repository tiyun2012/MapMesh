#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class MatchMeshShelfCmd : public MPxCommand {
public:
    MatchMeshShelfCmd() = default;
    ~MatchMeshShelfCmd() override = default;

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }

    static void* creator();
    static MSyntax newSyntax();
};
