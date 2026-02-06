loadPlugin "E:/dev/MAYA/API/C++/Autodesk_Maya_2026_3_Update_DEVKIT_Windows/devkitBase/build/neighboringVertices/Release/neighboringVertices.mll";

createNode neighboringVertices -n "nbr";
connectAttr -f pSphereShape1.outMesh nbr.inMesh;
connectAttr -f pSphereShape1.worldMatrix[0] nbr.worldMatrix;

setAttr nbr.position 0 0 0;
setAttr nbr.distance 1.0;

getAttr nbr.outVertexIds;
getAttr nbr.outCount;
getAttr nbr.closestPoint;
getAttr nbr.closestVertexId;


global proc mmSelectNeighborVerts(string $node)
{
    if (!`objExists $node`) {
        warning(("Node not found: " + $node));
        return;
    }

    string $meshShape[] = `listConnections -s 1 -d 0 ($node + ".inMesh")`;
    if (!size($meshShape)) {
        warning("No mesh connected to " + $node + ".inMesh");
        return;
    }

    int $ids[] = `getAttr ($node + ".outVertexIds")`;
    if (!size($ids)) {
        warning("No vertex ids to select.");
        select -cl;
        return;
    }

    string $sel[];
    for ($i = 0; $i < size($ids); ++$i) {
        $sel[$i] = ($meshShape[0] + ".vtx[" + $ids[$i] + "]");
    }

    select -r $sel;
}

mmSelectNeighborVerts("nbr");
