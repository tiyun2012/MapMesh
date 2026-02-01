#pragma once

#include <maya/MPxDeformerNode.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MTypeId.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MItGeometry.h>
#include <maya/MString.h>
#include <maya/MFnNurbsCurve.h>
#include <vector>
#include <unordered_map>

#include "RbfSolver.h"

class FitDeformerNode : public MPxDeformerNode {
public:
    FitDeformerNode() = default;
    ~FitDeformerNode() override = default;

    MStatus deform(MDataBlock& block,
                   MItGeometry& iter,
                   const MMatrix& localToWorld,
                   unsigned int geomIndex) override;

    static void* creator();
    static MStatus initialize();

    static MTypeId id;
    static MString typeName;
    static MObject aPinMatrices;     // matrix array: even=source, odd=target
    static MObject aSmoothWeight;    // float
    static MObject aRbfLambda;       // float
    static MObject aCurvePairs;      // compound array (sourceCurve, targetCurve)
    static MObject aSourceCurve;     // nurbsCurve
    static MObject aTargetCurve;     // nurbsCurve
    static MObject aCurveSamples;    // int

private:
    struct PinPair { MMatrix source; MMatrix target; };

    void rebuildRbf(const std::vector<PinPair>& pins, const MMatrix& worldToLocal, double lambda);
    MVector evaluateRbf(const MPoint& p) const;
    void computeAdjacency(const MObject& inputGeom);
    void laplacianSmooth(MPointArray& points, double weight) const;
    void sampleCurves(MDataBlock& block, std::vector<PinPair>& pins, double& hashAccum) const;

    matchmesh::RbfSolver m_rbf;
    size_t m_cachedSignature = 0;
    std::vector<std::vector<int>> m_adjacency;
};
