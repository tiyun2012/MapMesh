#include "FitDeformerNode.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MGlobal.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MString.h>
#include <algorithm>
#include <unordered_map>

#ifdef MATCHMESH_USE_TBB
    #include <tbb/parallel_for.h>
    #include <tbb/blocked_range.h>
#endif

MTypeId FitDeformerNode::id(0x00126b01);
MString FitDeformerNode::typeName("matchMeshDeformer");
MObject FitDeformerNode::aPinMatrices;
MObject FitDeformerNode::aSmoothWeight;
MObject FitDeformerNode::aRbfLambda;
MObject FitDeformerNode::aCurvePairs;
MObject FitDeformerNode::aSourceCurve;
MObject FitDeformerNode::aTargetCurve;
MObject FitDeformerNode::aCurveSamples;

void* FitDeformerNode::creator() { return new FitDeformerNode(); }

MStatus FitDeformerNode::initialize() {
    MStatus stat;
    MFnMatrixAttribute mAttr;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnCompoundAttribute cAttr;

    aPinMatrices = mAttr.create("pinMatrices", "pm", MFnMatrixAttribute::kDouble, &stat);
    mAttr.setArray(true);
    mAttr.setUsesArrayDataBuilder(true);
    mAttr.setStorable(true);
    mAttr.setConnectable(true);

    aSmoothWeight = nAttr.create("smoothWeight", "sw", MFnNumericData::kFloat, 0.35f, &stat);
    nAttr.setMin(0.0);
    nAttr.setMax(1.0);
    nAttr.setKeyable(true);

    aRbfLambda = nAttr.create("rbfLambda", "rl", MFnNumericData::kFloat, 0.001f, &stat);
    nAttr.setMin(0.0);
    nAttr.setSoftMax(0.1f);
    nAttr.setKeyable(true);

    aSourceCurve = tAttr.create("sourceCurve", "sc", MFnData::kNurbsCurve, &stat);
    aTargetCurve = tAttr.create("targetCurve", "tc", MFnData::kNurbsCurve, &stat);

    aCurvePairs = cAttr.create("curvePairs", "cp", &stat);
    cAttr.setArray(true);
    cAttr.setUsesArrayDataBuilder(true);
    cAttr.addChild(aSourceCurve);
    cAttr.addChild(aTargetCurve);

    aCurveSamples = nAttr.create("curveSamples", "cs", MFnNumericData::kInt, 20, &stat);
    nAttr.setMin(2);
    nAttr.setKeyable(true);

    addAttribute(aPinMatrices);
    addAttribute(aSmoothWeight);
    addAttribute(aRbfLambda);
    addAttribute(aCurvePairs);
    addAttribute(aCurveSamples);

    attributeAffects(aPinMatrices, outputGeom);
    attributeAffects(aSmoothWeight, outputGeom);
    attributeAffects(aRbfLambda, outputGeom);
    attributeAffects(aCurvePairs, outputGeom);
    attributeAffects(aCurveSamples, outputGeom);
    return MS::kSuccess;
}

void FitDeformerNode::rebuildRbf(const std::vector<PinPair>& pins, const MMatrix& worldToLocal, double lambda) {
    std::vector<matchmesh::PinSample> samples;
    samples.reserve(pins.size());
    for (const auto& p : pins) {
        MPoint src(0.0, 0.0, 0.0);
        MPoint tgt(0.0, 0.0, 0.0);
        src *= p.source;
        tgt *= p.target;
        // Convert to object space of deformer geometry
        src *= worldToLocal;
        tgt *= worldToLocal;
        matchmesh::PinSample s;
        s.source = src;
        s.delta = tgt - src;
        samples.push_back(s);
    }
    m_rbf.setPins(samples, lambda);
}

MVector FitDeformerNode::evaluateRbf(const MPoint& p) const {
    return m_rbf.evaluate(p);
}

void FitDeformerNode::computeAdjacency(const MObject& inputGeom) {
    m_adjacency.clear();
    if (inputGeom.isNull()) return;

    MStatus stat;
    MFnMesh fnMesh(inputGeom, &stat);
    if (stat != MS::kSuccess) return;

    const int vCount = fnMesh.numVertices();
    m_adjacency.resize(vCount);
    MItMeshPolygon itPoly(inputGeom);
    for (; !itPoly.isDone(); itPoly.next()) {
        MIntArray verts;
        itPoly.getVertices(verts);
        for (unsigned int i = 0; i < verts.length(); ++i) {
            int v0 = verts[i];
            int v1 = verts[(i + 1) % verts.length()];
            m_adjacency[v0].push_back(v1);
            m_adjacency[v1].push_back(v0);
        }
    }
}

void FitDeformerNode::laplacianSmooth(MPointArray& pts, double weight) const {
    if (m_adjacency.empty() || weight == 0.0) return;
    MPointArray deltas(pts.length(), MPoint::origin);

    auto computeRange = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            const auto& nbrs = m_adjacency[i];
            if (nbrs.empty()) continue;
            MPoint avg(0.0, 0.0, 0.0);
            for (int n : nbrs) avg += pts[n];
            double inv = 1.0 / static_cast<double>(nbrs.size());
            avg.x *= inv; avg.y *= inv; avg.z *= inv;
            deltas[i] = avg - pts[i];
        }
    };

    auto applyRange = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            pts[i] = pts[i] + deltas[i] * weight;
        }
    };

#ifdef MATCHMESH_USE_TBB
    tbb::parallel_for(tbb::blocked_range<size_t>(0, pts.length()),
        [&](const tbb::blocked_range<size_t>& r) {
            computeRange(static_cast<int>(r.begin()), static_cast<int>(r.end()));
        });
    tbb::parallel_for(tbb::blocked_range<size_t>(0, pts.length()),
        [&](const tbb::blocked_range<size_t>& r) {
            applyRange(static_cast<int>(r.begin()), static_cast<int>(r.end()));
        });
#else
    computeRange(0, static_cast<int>(pts.length()));
    applyRange(0, static_cast<int>(pts.length()));
#endif
}

void FitDeformerNode::sampleCurves(MDataBlock& block, std::vector<PinPair>& pins, double& hashAccum) const {
    MStatus status;
    MArrayDataHandle hPairs = block.inputArrayValue(aCurvePairs, &status);
    if (status != MS::kSuccess) return;

    int samples = block.inputValue(aCurveSamples).asInt();
    if (samples < 2) samples = 2;

    const unsigned int count = hPairs.elementCount();
    for (unsigned int i = 0; i < count; ++i) {
        hPairs.jumpToElement(i);
        MDataHandle hPair = hPairs.inputValue();
        MObject oSrc = hPair.child(aSourceCurve).asNurbsCurve();
        MObject oTgt = hPair.child(aTargetCurve).asNurbsCurve();
        if (oSrc.isNull() || oTgt.isNull()) continue;

        MStatus s1, s2;
        MFnNurbsCurve fnSrc(oSrc, &s1);
        MFnNurbsCurve fnTgt(oTgt, &s2);
        if (s1 != MS::kSuccess || s2 != MS::kSuccess) continue;

        double minP = 0.0, maxP = 1.0;
        fnSrc.getKnotDomain(minP, maxP);
        double step = (maxP - minP) / static_cast<double>(samples - 1);

        for (int k = 0; k < samples; ++k) {
            double param = minP + step * static_cast<double>(k);
            MPoint pSrc, pTgt;
            fnSrc.getPointAtParam(param, pSrc, MSpace::kWorld);
            fnTgt.getPointAtParam(param, pTgt, MSpace::kWorld);

            MMatrix mSrc, mTgt;
            mSrc[3][0] = pSrc.x; mSrc[3][1] = pSrc.y; mSrc[3][2] = pSrc.z;
            mTgt[3][0] = pTgt.x; mTgt[3][1] = pTgt.y; mTgt[3][2] = pTgt.z;
            pins.push_back({mSrc, mTgt});

            hashAccum += pSrc.x + pSrc.y + pSrc.z + pTgt.x + pTgt.y + pTgt.z;
        }
    }
}

MStatus FitDeformerNode::deform(MDataBlock& block,
                                MItGeometry& iter,
                                const MMatrix& localToWorld,
                                unsigned int geomIndex) {
    MStatus status;
    float env = block.inputValue(envelope, &status).asFloat();
    if (status != MS::kSuccess || env == 0.0f) return MS::kSuccess;

    MMatrix worldToLocal = localToWorld.inverse();

    MArrayDataHandle pinArray = block.inputArrayValue(aPinMatrices, &status);
    if (status != MS::kSuccess) return status;

    const unsigned int pinCount = pinArray.elementCount();
    std::vector<PinPair> pins;
    pins.reserve(pinCount / 2);

    // Build a map of existing logical indices to matrices to handle sparse arrays safely.
    std::unordered_map<unsigned int, MMatrix> pinMap;
    pinMap.reserve(pinCount);
    for (unsigned int i = 0; i < pinCount; ++i) {
        pinMap[pinArray.elementIndex()] = pinArray.inputValue().asMatrix();
        if (i + 1 < pinCount)
            pinArray.next();
    }

    std::vector<unsigned int> indices;
    indices.reserve(pinMap.size());
    for (const auto& kv : pinMap) indices.push_back(kv.first);
    std::sort(indices.begin(), indices.end());

    for (unsigned int idx : indices) {
        if (idx % 2 != 0)
            continue;
        auto it = pinMap.find(idx + 1);
        if (it == pinMap.end())
            continue;
        pins.push_back({pinMap[idx], it->second});
    }

    size_t signature = pins.size();
    double hash = 0.0;
    for (const auto& p : pins) {
        hash += p.source[3][0] + p.source[3][1] + p.source[3][2];
        hash += p.target[3][0] + p.target[3][1] + p.target[3][2];
    }

    // Add curve-sampled pins (optional)
    sampleCurves(block, pins, hash);
    signature = pins.size();

    size_t combined = static_cast<size_t>(signature * 73856093) ^ static_cast<size_t>(hash * 1e3);
    double lambda = block.inputValue(aRbfLambda, &status).asFloat();

    if (combined != m_cachedSignature) {
        rebuildRbf(pins, worldToLocal, lambda);
        m_cachedSignature = combined;
    }

    float smoothWeight = block.inputValue(aSmoothWeight, &status).asFloat();

    MObject inputGeomObj;
    {
        MArrayDataHandle hInput = block.inputArrayValue(input, &status);
        hInput.jumpToElement(geomIndex);
        MDataHandle hInputGeom = hInput.inputValue().child(inputGeom);
        inputGeomObj = hInputGeom.data();
    }
    if (status == MS::kSuccess && !inputGeomObj.isNull()) {
        computeAdjacency(inputGeomObj);
    }

    MPointArray pts;
    iter.allPositions(pts, MSpace::kObject);

    auto deformRange = [&](int start, int end) {
        for (int idx = start; idx < end; ++idx) {
            MPoint p = pts[idx];
            MVector delta = evaluateRbf(p) * env;
            pts[idx] = p + delta;
        }
    };

#ifdef MATCHMESH_USE_TBB
    tbb::parallel_for(tbb::blocked_range<size_t>(0, pts.length()),
        [&](const tbb::blocked_range<size_t>& r) {
            deformRange(static_cast<int>(r.begin()), static_cast<int>(r.end()));
        });
#else
    deformRange(0, static_cast<int>(pts.length()));
#endif

    if (smoothWeight > 0.0f) {
        laplacianSmooth(pts, smoothWeight * env);
    }

    iter.setAllPositions(pts, MSpace::kObject);
    return MS::kSuccess;
}
