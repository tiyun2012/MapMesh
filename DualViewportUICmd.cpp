#include "DualViewportUICmd.h"
#include "DualViewportCmd.h"

#include <maya/MGlobal.h>
#include <maya/MArgDatabase.h>
#include <maya/MString.h>
#include <sstream>

namespace {
const char* kCtrlName = "MatchMeshDualViewControl";
const char* kToolbarCtrlName = "MatchMeshToolbarControl";
const char* kLeftPanelName = "matchMeshTargetPanel";
const char* kRightPanelName = "matchMeshSourcePanel";

const char* kLeftFlag = "-l";
const char* kLeftLong = "-leftName";
const char* kRightFlag = "-r";
const char* kRightLong = "-rightName";
}

void* DualViewportUICmd::creator() { return new DualViewportUICmd(); }

MSyntax DualViewportUICmd::newSyntax() {
    MSyntax syntax;
    syntax.addFlag(kLeftFlag, kLeftLong, MSyntax::kString);
    syntax.addFlag(kRightFlag, kRightLong, MSyntax::kString);
    return syntax;
}

MStatus DualViewportUICmd::doIt(const MArgList& args) {
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MString leftName(kLeftPanelName);
    MString rightName(kRightPanelName);
    if (db.isFlagSet(kLeftFlag)) db.getFlagArgument(kLeftFlag, 0, leftName);
    if (db.isFlagSet(kRightFlag)) db.getFlagArgument(kRightFlag, 0, rightName);

    std::ostringstream oss;
    oss << "if (`exists matchMeshDeleteDualViewCams`) matchMeshDeleteDualViewCams();\n";
    oss << "if (`workspaceControl -exists " << kCtrlName << "`) deleteUI " << kCtrlName << ";\n";
    oss << "if (`workspaceControl -exists " << kToolbarCtrlName << "`) deleteUI " << kToolbarCtrlName << ";\n";
    oss << "if (`modelPanel -exists " << leftName.asChar() << "`) deleteUI -panel " << leftName.asChar() << ";\n";
    oss << "if (`modelPanel -exists " << rightName.asChar() << "`) deleteUI -panel " << rightName.asChar() << ";\n";
    // Floating/dockable toolbar workspace control (toolbox style).
    oss << "workspaceControl -label \"MatchMesh Tools\" -retain false -floating true -initialHeight 48 -initialWidth 140 "
        << kToolbarCtrlName << ";\n";
    oss << "columnLayout -p " << kToolbarCtrlName << " -adj true matchMeshToolbar;\n";
    // Pick icons (prefer Maya icons if present, fallback to built-ins).
    oss << "string $mmSrcIcon = \"polyCube.png\";\n";
    oss << "string $mmTgtIcon = \"polySphere.png\";\n";
    oss << "string $mmPinIcon = \"polyCube.png\";\n";
    oss << "string $mmMayaLoc = `getenv \"MAYA_LOCATION\"`;\n";
    oss << "if (size($mmMayaLoc)){\n";
    oss << "  string $pinIcon = ($mmMayaLoc + \"/icons/pin.png\");\n";
    oss << "  if (`filetest -f $pinIcon`) $mmPinIcon = $pinIcon;\n";
    oss << "}\n";
    oss << "iconTextButton -style \"iconOnly\" -image1 $mmSrcIcon -w 36 -h 36 "
           "-ann \"Set Source Mesh (select a mesh transform)\" "
           "-c \"matchMeshSetSourceMesh;\" matchMeshSetSourceBtn;\n";
    oss << "iconTextButton -style \"iconOnly\" -image1 $mmTgtIcon -w 36 -h 36 "
           "-ann \"Set Target Mesh (select a mesh transform)\" "
           "-c \"matchMeshSetTargetMesh;\" matchMeshSetTargetBtn;\n";
    oss << "iconTextButton -style \"iconOnly\" -image1 $mmPinIcon -w 36 -h 36 "
           "-ann \"Create pin (no selection = origin; one component = both; two components = source/target)\" "
           "-c \"matchMeshCreatePinFromSelection;\" matchMeshCreatePinBtn;\n";
    oss << "setParent ..;\n";

    // Main dual-view workspace control.
    oss << "workspaceControl -label \"MatchMesh Studio\" -retain false " << kCtrlName << ";\n";
    oss << "formLayout -p " << kCtrlName << " matchMeshRoot;\n";
    // Main split: left/right viewports only.
    oss << "paneLayout -p matchMeshRoot -configuration \"vertical2\" matchMeshPane;\n";
    // Ensure each panel has its own camera so pan/zoom are independent.
    // Always create fresh, uniquely named cameras to avoid Maya auto-renaming and stale reuse.
    oss << "global string $gMatchMeshLeftCam[];\n";
    oss << "global string $gMatchMeshRightCam[];\n";
    oss << "$gMatchMeshLeftCam = `camera -name \"matchMeshLeftCam\"`;\n";
    oss << "$gMatchMeshRightCam = `camera -name \"matchMeshRightCam\"`;\n";
    // Default camera distance for both panels.
    oss << "setAttr ($gMatchMeshLeftCam[0] + \".translateZ\") 10.853;\n";
    oss << "setAttr ($gMatchMeshRightCam[0] + \".translateZ\") 10.853;\n";
    // Hide camera transforms in the scene.
    oss << "setAttr ($gMatchMeshLeftCam[0] + \".visibility\") 0;\n";
    oss << "setAttr ($gMatchMeshRightCam[0] + \".visibility\") 0;\n";
    oss << "modelPanel -p matchMeshPane -label \"Target Mesh\" -mbv false " << leftName.asChar() << ";\n";
    oss << "modelEditor -e -grid false -joints false -da \"smoothShaded\" -dtx true -camera $gMatchMeshLeftCam[1] "
        << leftName.asChar() << ";\n";
    oss << "modelPanel -p matchMeshPane -label \"Source Mesh\" -mbv false " << rightName.asChar() << ";\n";
    oss << "modelEditor -e -grid false -joints false -da \"smoothShaded\" -dtx true -camera $gMatchMeshRightCam[1] "
        << rightName.asChar() << ";\n";
    oss << "setParent matchMeshRoot;\n"; // end layout
    oss << "formLayout -e "
           "-attachForm matchMeshPane \"top\" 0 "
           "-attachForm matchMeshPane \"left\" 0 "
           "-attachForm matchMeshPane \"right\" 0 "
           "-attachForm matchMeshPane \"bottom\" 0 "
           "matchMeshRoot;\n";
    // Restore the workspace control without passing boolean args to flags that take no values.
    oss << "workspaceControl -e -restore " << kCtrlName << ";\n";
    // Ensure cameras are cleaned up when the UI is closed by the user.
    oss << "if (`exists matchMeshDeleteDualViewCams`) scriptJob -uiDeleted " << kCtrlName
        << " \"matchMeshDeleteDualViewCams\";\n";
    // Dock toolbar to the left of the main control if possible; otherwise it remains floating.
    oss << "catchQuiet(`workspaceControl -e -dockToControl " << kCtrlName << " left " << kToolbarCtrlName << "`);\n";
    // No auto-sync; each panel uses its own camera.

    MString cmd(oss.str().c_str());
    status = MGlobal::executeCommand(cmd, false, true);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    return MS::kSuccess;
}
