// locatorFollowDeformer.cpp
// Vertices follow a locator/transform by the locator's *delta translation* from bind.
// Influence = falloff(dist to current locator, radius) * envelope * paintedWeight * strength
//
// Usage:
// 1) create deformer on mesh
// 2) connect locator.worldMatrix[0] -> targetMatrix
// 3) set bindMatrix once (usually at creation) to locator.worldMatrix[0]

#include <maya/MPxDeformerNode.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MItGeometry.h>
#include <maya/MPoint.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <algorithm>

class LocatorFollowDeformer : public MPxDeformerNode
{
public:
    static void* creator() { return new LocatorFollowDeformer(); }
    static MStatus initialize();

    MStatus deform(MDataBlock& block,
                   MItGeometry& iter,
                   const MMatrix& localToWorld,
                   unsigned int geomIndex) override;

    static MTypeId id;

    // attrs
    static MObject aTargetMatrix; // connect locator.worldMatrix[0]
    static MObject aBindMatrix;   // set once to locator.worldMatrix[0] at bind time
    static MObject aRadius;       // world-space influence radius
    static MObject aStrength;     // multiplier
};

MTypeId LocatorFollowDeformer::id(0x0012F2A1); // change to your own unique ID
MObject LocatorFollowDeformer::aTargetMatrix;
MObject LocatorFollowDeformer::aBindMatrix;
MObject LocatorFollowDeformer::aRadius;
MObject LocatorFollowDeformer::aStrength;

static inline double clamp01(double v)
{
    return (v < 0.0) ? 0.0 : (v > 1.0 ? 1.0 : v);
}

// smoothstep 0..1
static inline double smoothstep(double t)
{
    t = clamp01(t);
    return t * t * (3.0 - 2.0 * t);
}

MStatus LocatorFollowDeformer::deform(MDataBlock& block,
                                     MItGeometry& iter,
                                     const MMatrix& localToWorld,
                                     unsigned int geomIndex)
{
    MStatus status;

    const float env = block.inputValue(envelope, &status).asFloat();
    if (status != MS::kSuccess || env <= 0.0f) return MS::kSuccess;

    // IMPORTANT: avoid "pull to origin" if user hasn't connected the target yet
    {
        MPlug tPlug(thisMObject(), aTargetMatrix);
        if (!tPlug.isConnected())
            return MS::kSuccess;
    }

    const double radius = block.inputValue(aRadius, &status).asDouble();
    if (status != MS::kSuccess || radius <= 1e-8) return MS::kSuccess;

    const double strength = block.inputValue(aStrength, &status).asDouble();
    if (status != MS::kSuccess || strength <= 0.0) return MS::kSuccess;

    const MMatrix targetM = block.inputValue(aTargetMatrix, &status).asMatrix();
    if (status != MS::kSuccess) return MS::kSuccess;

    const MMatrix bindM = block.inputValue(aBindMatrix, &status).asMatrix();
    if (status != MS::kSuccess) return MS::kSuccess;

    const MVector targetPos = MTransformationMatrix(targetM).getTranslation(MSpace::kWorld);
    const MVector bindPos   = MTransformationMatrix(bindM).getTranslation(MSpace::kWorld);

    // If bindMatrix is still identity (never set), do nothing (prevents initial jump)
    // You should set bindMatrix once via MEL/Python when you create the deformer.
    const bool bindLooksUnset =
        bindM.isEquivalent(MMatrix::identity, 1e-10);
    if (bindLooksUnset)
        return MS::kSuccess;

    const MVector delta = targetPos - bindPos; // locator movement since bind

    const MMatrix worldToLocal = localToWorld.inverse();

    for (; !iter.isDone(); iter.next())
    {
        const int vtxId = iter.index();
        const float paintedW = weightValue(block, geomIndex, vtxId);
        if (paintedW <= 0.0f) continue;

        MPoint pLocal = iter.position();
        MPoint pWorld = pLocal * localToWorld;

        // falloff based on distance to CURRENT locator position
        const double dist = (MVector(pWorld) - targetPos).length();
        const double t = 1.0 - (dist / radius);     // 1 at center, 0 at radius
        const double falloff = smoothstep(t);
        if (falloff <= 0.0) continue;

        const double w = clamp01(falloff * strength) * (double)env * (double)paintedW;

        // Apply only the locator delta (translation follow)
        MPoint newWorld = pWorld + delta * w;

        MPoint newLocal = newWorld * worldToLocal;
        iter.setPosition(newLocal);
    }

    return MS::kSuccess;
}

MStatus LocatorFollowDeformer::initialize()
{
    MStatus status;

    MFnMatrixAttribute mAttr;
    aTargetMatrix = mAttr.create("targetMatrix", "tmat", MFnMatrixAttribute::kDouble, &status);
    mAttr.setStorable(false);
    mAttr.setKeyable(false);
    mAttr.setReadable(true);
    mAttr.setWritable(true);
    mAttr.setConnectable(true);
    addAttribute(aTargetMatrix);

    aBindMatrix = mAttr.create("bindMatrix", "bmat", MFnMatrixAttribute::kDouble, &status);
    mAttr.setStorable(true);
    mAttr.setKeyable(false);
    mAttr.setReadable(true);
    mAttr.setWritable(true);
    mAttr.setConnectable(false);
    addAttribute(aBindMatrix);

    MFnNumericAttribute nAttr;
    aRadius = nAttr.create("radius", "rad", MFnNumericData::kDouble, 5.0, &status);
    nAttr.setMin(0.000001);
    nAttr.setKeyable(true);
    addAttribute(aRadius);

    aStrength = nAttr.create("strength", "str", MFnNumericData::kDouble, 1.0, &status);
    nAttr.setMin(0.0);
    nAttr.setKeyable(true);
    addAttribute(aStrength);

    attributeAffects(aTargetMatrix, outputGeom);
    attributeAffects(aBindMatrix, outputGeom);
    attributeAffects(aRadius, outputGeom);
    attributeAffects(aStrength, outputGeom);

    return MS::kSuccess;
}

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "YourName", "1.0", "Any");
    return plugin.registerNode(
        "locatorFollowDeformer",
        LocatorFollowDeformer::id,
        LocatorFollowDeformer::creator,
        LocatorFollowDeformer::initialize,
        MPxNode::kDeformerNode
    );
}

MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);
    return plugin.deregisterNode(LocatorFollowDeformer::id);
}
