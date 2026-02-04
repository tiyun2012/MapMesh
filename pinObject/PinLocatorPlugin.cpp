// PinLocatorPlugin.cpp
#include "PinLocator.h"

#include <maya/MFnPlugin.h>
#include <maya/MDrawRegistry.h>

MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, "YourStudio", "1.0", "Any");

    status = plugin.registerNode("PinLocator",
                                 PinLocatorNode::id,
                                 PinLocatorNode::creator,
                                 PinLocatorNode::initialize,
                                 MPxNode::kLocatorNode,
                                 &PinLocatorNode::drawDbClassification);
    if (!status) {
        status.perror("registerNode PinLocator");
        return status;
    }

    status = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
        PinLocatorNode::drawDbClassification,
        PinLocatorNode::drawRegistrantId,
        PinDrawOverride::creator);
    if (!status) {
        status.perror("registerDrawOverrideCreator PinLocator");
        plugin.deregisterNode(PinLocatorNode::id);
        return status;
    }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj);

    status = MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(
        PinLocatorNode::drawDbClassification,
        PinLocatorNode::drawRegistrantId);
    if (!status) {
        status.perror("deregisterDrawOverrideCreator PinLocator");
    }

    MStatus nodeStatus = plugin.deregisterNode(PinLocatorNode::id);
    if (!nodeStatus) {
        nodeStatus.perror("deregisterNode PinLocator");
    }

    return (status == MS::kSuccess) ? nodeStatus : status;
}
