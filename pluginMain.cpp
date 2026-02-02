#include <maya/MFnPlugin.h>
#include <maya/MDrawRegistry.h>
#include <maya/MGlobal.h>

#include "PinLocatorNode.h"
#include "MatchMeshCreatePinCmd.h"
#include "DualViewportUICmd.h"

MStatus initializePlugin(MObject obj) {
    MStatus status;
    MFnPlugin plugin(obj, "MatchMesh", "1.0", "Any");

    status = plugin.registerNode("MatchMeshPin",
                                 PinLocatorNode::id,
                                 PinLocatorNode::creator,
                                 PinLocatorNode::initialize,
                                 MPxNode::kLocatorNode,
                                 &PinLocatorNode::drawDbClassification);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
        PinLocatorNode::drawDbClassification,
        PinLocatorNode::drawRegistrantId,
        PinDrawOverride::creator);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = plugin.registerCommand("matchMeshCreatePin",
                                    MatchMeshCreatePinCmd::creator,
                                    MatchMeshCreatePinCmd::newSyntax);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = plugin.registerCommand("matchMeshDualViewUI", DualViewportUICmd::creator, DualViewportUICmd::newSyntax);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Inject MEL helpers for isolate + camera sync (robust against stale scriptJobs)
    const char* melHelpers = R"mel(
global proc matchMeshDeleteDualViewCams(){
  // Delete any cameras created by MatchMesh dual view.
  string $camXforms[] = `ls -type transform "matchMeshLeftCam*" "matchMeshRightCam*" "matchMeshTargetCam*" "matchMeshSourceCam*"`;
  for ($c in $camXforms){
    if (`objExists $c`)
      delete $c;
  }
  // Clean up any orphan camera shapes that might remain.
  string $camShapes[] = `ls -type camera "matchMeshLeftCam*" "matchMeshRightCam*" "matchMeshTargetCam*" "matchMeshSourceCam*"`;
  for ($s in $camShapes){
    if (!`objExists $s`)
      continue;
    string $parents[] = `listRelatives -p $s`;
    if (size($parents))
      delete $parents[0];
    else
      delete $s;
  }
}

global proc matchMeshAssignSet(string $setName){
  string $sel[] = `ls -sl -long`;
  if (!size($sel)){
    warning("MatchMesh: select a mesh transform.");
    return;
  }
  string $node = $sel[0];
  if (`nodeType $node` == "mesh"){
    string $parents[] = `listRelatives -p $node`;
    if (size($parents)) $node = $parents[0];
  }
  string $shapes[] = `listRelatives -s -ni -f $node`;
  int $hasMesh = 0;
  for ($s in $shapes){
    if (`nodeType $s` == "mesh"){
      $hasMesh = 1; break;
    }
  }
  if (!$hasMesh){
    warning("MatchMesh: selected object has no mesh shape.");
    return;
  }
  if (!`objExists $setName`)
    sets -em -name $setName;
  catchQuiet(`lockNode -l 0 $setName`);
  sets -e -clear $setName;
  sets -e -forceElement $setName $node;
  lockNode -l 1 $setName;
}

global proc matchMeshSetSourceMesh(){ matchMeshAssignSet("MatchMeshSourceSet"); }
global proc matchMeshSetTargetMesh(){ matchMeshAssignSet("MatchMeshTargetSet"); }

global proc matchMeshCreatePinFromSelection(){
  if (!`objExists "MatchMeshSourceSet"` || !`objExists "MatchMeshTargetSet"`){
    warning("MatchMesh: set Source and Target meshes first.");
    return;
  }
  matchMeshCreatePin -sourceSet "MatchMeshSourceSet" -targetSet "MatchMeshTargetSet";
}
)mel";
    MGlobal::executeCommand(melHelpers, false, true);

    return status;
}

MStatus uninitializePlugin(MObject obj) {
    MStatus status;
    MFnPlugin plugin(obj);

    // Clean up UI and nodes before deregistering commands/nodes.
    MGlobal::executeCommand(
        "if (`workspaceControl -exists MatchMeshDualViewControl`) deleteUI MatchMeshDualViewControl;"
        "if (`workspaceControl -exists MatchMeshToolbarControl`) deleteUI MatchMeshToolbarControl;"
        "if (`exists matchMeshDeleteDualViewCams`) matchMeshDeleteDualViewCams();"
        "if (`shelfLayout -exists MatchMesh`) deleteUI MatchMesh;"
        // Delete any remaining nodes to avoid dangling draw overrides on unload.
        "string $mmPins[] = `ls -type MatchMeshPin`;"
        "if (size($mmPins)) delete $mmPins;",
        true, true);

    status = plugin.deregisterCommand("matchMeshCreatePin");
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = plugin.deregisterCommand("matchMeshDualViewUI");
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
        PinLocatorNode::drawDbClassification,
        PinLocatorNode::drawRegistrantId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    status = plugin.deregisterNode(PinLocatorNode::id);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return status;
}
