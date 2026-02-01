#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class DualViewportUICmd : public MPxCommand {
public:
    DualViewportUICmd() = default;
    ~DualViewportUICmd() override = default;

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override { return false; }

    static void* creator();
    static MSyntax newSyntax();
};
