#include "MatchMeshShelfCmd.h"

#include <maya/MGlobal.h>
#include <maya/MArgDatabase.h>
#include <maya/MString.h>

namespace {
const char* kShelfFlag = "-s";
const char* kShelfLong = "-shelf";
}

void* MatchMeshShelfCmd::creator() { return new MatchMeshShelfCmd(); }

MSyntax MatchMeshShelfCmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kShelfFlag, kShelfLong, MSyntax::kString);
    return syntax;
}

MStatus MatchMeshShelfCmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString shelfName("MatchMesh");
    if (db.isFlagSet(kShelfFlag)) db.getFlagArgument(kShelfFlag, 0, shelfName);

    MString cmd;
    cmd += "global string $gShelfTopLevel;\n";
    cmd += "if (!`shelfLayout -exists " + shelfName + "`) {\n";
    cmd += "    shelfLayout -p $gShelfTopLevel " + shelfName + ";\n";
    cmd += "}\n";
    cmd += "string $icon = `internalVar -usd` + \"commandButton.png\";\n";
    cmd += "shelfButton -p " + shelfName + " -i $icon -l \"MatchMesh\" "
           "-ann \"Open MatchMesh dual-view UI\" "
           "-command \"matchMeshDualViewUI\";\n";

    status = MGlobal::executeCommand(cmd, false, true);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    return MS::kSuccess;
}
