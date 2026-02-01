#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

class DualViewportCmd : public MPxCommand {
public:
    DualViewportCmd() = default;
    ~DualViewportCmd() override = default;

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }

    static void* creator();
    static MSyntax newSyntax();
};
