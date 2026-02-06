# -*- coding: utf-8 -*-

"""MatchMesh UI (Python)

Usage:
    from maya import cmds
    import matchmesh_ui
    matchmesh_ui.show()

This script recreates the same UI (workspace controls + two modelPanels + toolbox)
that the old C++ command builds, but in Python so you can iterate fast.

It calls the SAME MEL/helpers/commands as your original UI:
- matchMeshSetSourceMesh
- matchMeshSetTargetMesh
- matchMeshCreatePinFromSelection
- matchMeshDebugClosestFace
- matchMeshMoveFacesAlongPinVector
"""

from __future__ import annotations

import os
from maya import cmds, mel

# --- Names (match the plugin/C++ UI) ---
CTRL_NAME = "MatchMeshDualViewControl"
TOOLBAR_CTRL_NAME = "MatchMeshToolbarControl"
LEFT_PANEL_DEFAULT = "matchMeshTargetPanel"
RIGHT_PANEL_DEFAULT = "matchMeshSourcePanel"

# Cameras (match the C++ UI defaults)
TARGET_CAM_XFORM = "matchMeshTargetCam"
SOURCE_CAM_XFORM = "matchMeshSourceCam"


# ------------------------------------------------------------
# Small helpers
# ------------------------------------------------------------

def _mel_proc_exists(proc_name: str) -> bool:
    try:
        # mel `exists "proc"` returns 0/1
        return bool(mel.eval('exists "%s"' % proc_name))
    except Exception:
        return False


def _safe_delete_workspace_control(name: str):
    try:
        if cmds.workspaceControl(name, exists=True):
            cmds.deleteUI(name)
    except Exception:
        pass


def _safe_delete_model_panel(name: str):
    try:
        if cmds.modelPanel(name, exists=True):
            cmds.deleteUI(name, panel=True)
    except Exception:
        pass


def delete_dualview_cams():
    """Delete MatchMesh dual view cameras if present."""
    if _mel_proc_exists("matchMeshDeleteDualViewCams"):
        try:
            mel.eval("matchMeshDeleteDualViewCams()")
            return
        except Exception:
            pass

    # Fallback if MEL helper is not available
    for xform in (TARGET_CAM_XFORM, SOURCE_CAM_XFORM):
        try:
            if cmds.objExists(xform):
                cmds.delete(xform)
        except Exception:
            pass


def _dock_workspace_control(child: str, target: str, where: str):
    """Best-effort docking. where: left/right/top/bottom"""
    try:
        # Some Maya versions accept dockToControl=(target, where)
        cmds.workspaceControl(child, e=True, dockToControl=(target, where))
        return
    except Exception:
        pass

    # MEL fallback (matches what your C++ UI did)
    try:
        mel.eval('catchQuiet(`workspaceControl -e -dockToControl %s %s %s`);' % (target, where, child))
    except Exception:
        pass


def _icon_path(preferred: str, fallback: str) -> str:
    """Return an icon path usable by iconTextButton."""
    maya_loc = os.environ.get("MAYA_LOCATION", "")
    if maya_loc:
        p = os.path.join(maya_loc, "icons", preferred)
        if os.path.isfile(p):
            return p
    return fallback


# ------------------------------------------------------------
# UI builders
# ------------------------------------------------------------

def _build_toolbar():
    # Floating/dockable toolbar workspace control (toolbox style)
    cmds.workspaceControl(
        TOOLBAR_CTRL_NAME,
        label="MatchMesh Tools",
        retain=False,
        floating=True,
        initialHeight=48,
        initialWidth=140,
    )

    cmds.columnLayout("matchMeshToolbar", p=TOOLBAR_CTRL_NAME, adj=True)

    mm_src_icon = _icon_path("polyCube.png", "polyCube.png")
    mm_tgt_icon = _icon_path("polySphere.png", "polySphere.png")
    mm_pin_icon = _icon_path("pin.png", "polyCube.png")
    mm_dbg_icon = _icon_path("locator.png", "locator.png")
    mm_move_icon = _icon_path("TypeMoveTool.png", "TypeMoveTool.png")

    cmds.iconTextButton(
        "matchMeshSetSourceBtn",
        style="iconOnly",
        image1=mm_src_icon,
        w=36,
        h=36,
        ann="Set Source Mesh (select a mesh transform)",
        c="matchMeshSetSourceMesh;",
    )
    cmds.iconTextButton(
        "matchMeshSetTargetBtn",
        style="iconOnly",
        image1=mm_tgt_icon,
        w=36,
        h=36,
        ann="Set Target Mesh (select a mesh transform)",
        c="matchMeshSetTargetMesh;",
    )
    cmds.iconTextButton(
        "matchMeshCreatePinBtn",
        style="iconOnly",
        image1=mm_pin_icon,
        w=36,
        h=36,
        ann=(
            "Create pin (no selection = origin; one component = both; "
            "two components = source/target)"
        ),
        c="matchMeshCreatePinFromSelection;",
    )

    # Debug row
    cmds.rowLayout(nc=2, cw2=(60, 36), ct2=("left", "left"))
    cmds.floatField(
        "matchMeshDbgRadius",
        w=60,
        value=0.0,
        precision=2,
        ann="Debug radius",
    )
    cmds.iconTextButton(
        "matchMeshDebugClosestBtn",
        style="iconOnly",
        image1=mm_dbg_icon,
        w=36,
        h=36,
        ann="Debug closest face on source mesh from selected pin",
        c=(
            "float $r=`floatField -q -v matchMeshDbgRadius`; "
            "matchMeshDebugClosestFace -clear -r $r;"
        ),
    )
    cmds.setParent("..")

    # Move row
    cmds.rowLayout(nc=2, cw2=(60, 36), ct2=("left", "left"))
    cmds.floatField(
        "matchMeshMoveStep",
        w=60,
        value=0.1,
        precision=3,
        ann="Move step",
    )
    cmds.iconTextButton(
        "matchMeshMoveFacesBtn",
        style="iconOnly",
        image1=mm_move_icon,
        w=36,
        h=36,
        ann="Move faces on source mesh along source pin vectors",
        c=(
            "float $r=`floatField -q -v matchMeshDbgRadius`; "
            "float $s=`floatField -q -v matchMeshMoveStep`; "
            "matchMeshMoveFacesAlongPinVector -r $r -s $s;"
        ),
    )
    cmds.setParent("..")

    cmds.setParent("..")  # matchMeshToolbar



def _build_dual_view(left_panel: str, right_panel: str):
    # Main dual-view workspace control
    cmds.workspaceControl(CTRL_NAME, label="MatchMesh Studio", retain=False)

    cmds.formLayout("matchMeshRoot", p=CTRL_NAME)
    cmds.paneLayout("matchMeshPane", p="matchMeshRoot", configuration="vertical2")

    # Create fresh cameras (like the C++ UI) so pan/zoom are independent.
    tgt_xform, tgt_shape = cmds.camera(name=TARGET_CAM_XFORM)
    src_xform, src_shape = cmds.camera(name=SOURCE_CAM_XFORM)

    for cam_xform in (tgt_xform, src_xform):
        try:
            cmds.setAttr(cam_xform + ".translateZ", 10.853)
            cmds.setAttr(cam_xform + ".visibility", 0)
        except Exception:
            pass

    # Panels
    cmds.modelPanel(left_panel, p="matchMeshPane", label="Target Mesh", mbv=False)
    cmds.modelEditor(
        left_panel,
        e=True,
        grid=False,
        joints=False,
        da="smoothShaded",
        dtx=True,
        camera=tgt_shape,
    )

    cmds.modelPanel(right_panel, p="matchMeshPane", label="Source Mesh", mbv=False)
    cmds.modelEditor(
        right_panel,
        e=True,
        grid=False,
        joints=False,
        da="smoothShaded",
        dtx=True,
        camera=src_shape,
    )

    # Fill the form
    cmds.setParent("matchMeshRoot")
    cmds.formLayout(
        "matchMeshRoot",
        e=True,
        attachForm=[
            ("matchMeshPane", "top", 0),
            ("matchMeshPane", "left", 0),
            ("matchMeshPane", "right", 0),
            ("matchMeshPane", "bottom", 0),
        ],
    )

    # Restore workspace state (safe if floating/docked)
    try:
        cmds.workspaceControl(CTRL_NAME, e=True, restore=True)
    except Exception:
        pass

    # Cleanup cameras when UI is closed
    if _mel_proc_exists("matchMeshDeleteDualViewCams"):
        try:
            cmds.scriptJob(uiDeleted=(CTRL_NAME, "matchMeshDeleteDualViewCams"))
        except Exception:
            pass



def dock_layer_modifiers(layer_module: str = "layerWidget"):
    """Optional: show your layer widget and dock it next to the MatchMesh tools.

    This is best-effort. It will:
    - import layer_module and call show_ui() if present
    - then dock any resulting workspaceControl to the toolbar's right
    """
    try:
        import importlib
        mod = importlib.import_module(layer_module)
        importlib.reload(mod)
        if hasattr(mod, "show_ui"):
            mod.show_ui()
        elif hasattr(mod, "show"):
            mod.show()
    except Exception:
        cmds.warning("MatchMesh UI: couldn't import/show layer module: %s" % layer_module)
        return

    # Dock any workspace control that looks like the layer modifiers
    try:
        wcs = cmds.lsUI(type="workspaceControl") or []
    except Exception:
        wcs = []

    candidates = [
        w for w in wcs
        if "Layer" in w or "layer" in w or "Modifier" in w or "modifier" in w
    ]
    # Prefer exact names if you use them
    preferred = [w for w in candidates if "MatchMesh" in w]
    wc_name = preferred[0] if preferred else (candidates[0] if candidates else None)

    if wc_name and cmds.workspaceControl(wc_name, exists=True):
        _dock_workspace_control(wc_name, TOOLBAR_CTRL_NAME, "right")


# ------------------------------------------------------------
# Public entrypoint
# ------------------------------------------------------------

def show(
    left_panel: str = LEFT_PANEL_DEFAULT,
    right_panel: str = RIGHT_PANEL_DEFAULT,
    dock_toolbar: bool = True,
    also_show_layer_modifiers: bool = False,
    layer_module: str = "layerWidget",
):
    """Create (or recreate) the MatchMesh Studio UI."""

    # Cleanup previous instance
    try:
        delete_dualview_cams()
    except Exception:
        pass

    _safe_delete_workspace_control(CTRL_NAME)
    _safe_delete_workspace_control(TOOLBAR_CTRL_NAME)
    _safe_delete_model_panel(left_panel)
    _safe_delete_model_panel(right_panel)

    # Build UI
    _build_toolbar()
    _build_dual_view(left_panel, right_panel)

    if dock_toolbar:
        _dock_workspace_control(TOOLBAR_CTRL_NAME, CTRL_NAME, "left")

    if also_show_layer_modifiers:
        dock_layer_modifiers(layer_module=layer_module)

    return CTRL_NAME
