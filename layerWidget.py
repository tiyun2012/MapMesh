# -*- coding: utf-8 -*-
"""
Layer Modifiers UI (PySide6) - Pro UX version
- Group is a "card" (QFrame) with a real header bar (no QGroupBox title float)
- Drag ONLY from the handle
- Clear drop indicator line
- Selection highlight
- F2 rename, Delete remove (with confirm)
- Search filter
- Collapse groups

Tested structure-wise for Maya + PySide6 (MayaQWidgetDockableMixin)
"""

from PySide6 import QtWidgets, QtCore, QtGui
from maya.app.general.mayaMixin import MayaQWidgetDockableMixin
import maya.cmds as cmds


# ------------------------------------------------------------
# Layer Widget
# ------------------------------------------------------------
class LayerWidget(QtWidgets.QWidget):
    clicked = QtCore.Signal(object)          # emits self
    request_delete = QtCore.Signal(object)   # emits self
    request_rename = QtCore.Signal(object)   # emits self

    def __init__(self, layer_name, parent=None):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.setObjectName("LayerWidget")
        self.setProperty("selected", False)

        self.setStyleSheet("""
            #LayerWidget {
                background-color: #383838;
                border: 1px solid #505050;
                border-radius: 6px;
            }
            #LayerWidget:hover {
                border: 1px solid #707070;
                background-color: #3e3e3e;
            }
            #LayerWidget[selected="true"] {
                border: 1px solid #4aa3ff;
                background-color: #2f3b46;
            }
            QLabel {
                color: #e0e0e0;
                background: transparent;
                border: none;
            }
        """)

        self.drag_start_pos = None

        # Layout
        self.main_layout = QtWidgets.QHBoxLayout(self)
        self.main_layout.setContentsMargins(8, 6, 8, 6)
        self.main_layout.setSpacing(8)

        # Drag handle (only this starts drag)
        self.drag_handle = QtWidgets.QLabel("⋮")
        self.drag_handle.setFixedWidth(16)
        self.drag_handle.setAlignment(QtCore.Qt.AlignCenter)
        self.drag_handle.setCursor(QtCore.Qt.SizeAllCursor)
        self.drag_handle.setObjectName("DragHandle")
        self.drag_handle.setStyleSheet("""
            #DragHandle { color:#8a8a8a; font-size:12px; }
            #DragHandle:hover { color:#cfcfcf; }
        """)
        self.drag_handle.installEventFilter(self)

        # Layer label
        self.label = QtWidgets.QLabel(layer_name)
        self.label.setStyleSheet("font-weight: 700;")
        self.label.setCursor(QtCore.Qt.PointingHandCursor)

        # Weight readout (local | real)
        self.value_label = QtWidgets.QLabel("")
        self.value_label.setFixedWidth(80)
        self.value_label.setStyleSheet("color:#bdbdbd; font-size:10px;")

        # Slider
        self.slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.slider.setRange(0, 100)
        self.slider.setValue(100)
        self.slider.setFixedWidth(110)
        self.slider.setStyleSheet("""
            QSlider::groove:horizontal {
                border: 0px;
                height: 2px;
                background: #222;
            }
            QSlider::sub-page:horizontal {
                background: #6688aa;
                height: 2px;
            }
            QSlider::handle:horizontal {
                background: #AAA;
                border: 1px solid #555;
                width: 12px;
                height: 12px;
                margin: -5px 0;
                border-radius: 6px;
            }
        """)

        # Buttons (rename/delete) to feel pro
        self.btn_rename = QtWidgets.QToolButton()
        self.btn_rename.setText("✎")
        self.btn_rename.setToolTip("Rename (F2)")
        self.btn_rename.setAutoRaise(True)

        self.btn_delete = QtWidgets.QToolButton()
        self.btn_delete.setText("✕")
        self.btn_delete.setToolTip("Delete (Del)")
        self.btn_delete.setAutoRaise(True)

        for b in (self.btn_rename, self.btn_delete):
            b.setCursor(QtCore.Qt.PointingHandCursor)
            b.setStyleSheet("""
                QToolButton { color:#bdbdbd; padding:0 4px; border-radius:4px; }
                QToolButton:hover { background:#4a4a4a; color:#ffffff; }
            """)

        self.main_layout.addWidget(self.drag_handle)
        self.main_layout.addWidget(self.label)
        self.main_layout.addStretch()
        self.main_layout.addWidget(self.value_label)
        self.main_layout.addWidget(self.slider)
        self.main_layout.addWidget(self.btn_rename)
        self.main_layout.addWidget(self.btn_delete)

        # Signals
        self.slider.valueChanged.connect(self.on_slider_change)
        self.btn_delete.clicked.connect(lambda: self.request_delete.emit(self))
        self.btn_rename.clicked.connect(lambda: self.request_rename.emit(self))

        # Context menu
        self.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self.show_context_menu)

        # Init
        self.on_slider_change(self.slider.value())

    def set_selected(self, state: bool):
        self.setProperty("selected", state)
        self.style().unpolish(self)
        self.style().polish(self)
        self.update()

    def get_layer_weight(self):
        return self.slider.value() / 100.0

    def get_real_weight(self):
        local_val = self.get_layer_weight()
        group_val = 1.0
        p = self.parent()
        while p:
            if isinstance(p, GroupWidget):
                group_val = p.get_group_weight()
                break
            p = p.parent()
        return local_val * group_val

    def on_slider_change(self, value):
        local_val = value / 100.0
        real_val = self.get_real_weight()
        self.value_label.setText(f"{local_val:.2f} | {real_val:.2f}")
        self.slider.setToolTip(f"Local: {local_val:.2f}\nReal: {real_val:.2f}")

    # Selection click
    def mousePressEvent(self, event):
        if event.button() == QtCore.Qt.LeftButton:
            self.clicked.emit(self)
        super().mousePressEvent(event)

    # Double click rename
    def mouseDoubleClickEvent(self, event):
        if event.button() == QtCore.Qt.LeftButton:
            self.request_rename.emit(self)
        super().mouseDoubleClickEvent(event)

    def show_context_menu(self, pos):
        menu = QtWidgets.QMenu(self)
        menu.addAction("Rename Layer").triggered.connect(lambda: self.request_rename.emit(self))
        menu.addAction("Delete Layer").triggered.connect(lambda: self.request_delete.emit(self))
        menu.exec_(self.mapToGlobal(pos))

    # Drag ONLY via handle
    def eventFilter(self, obj, event):
        if obj is self.drag_handle:
            if event.type() == QtCore.QEvent.MouseButtonPress and event.button() == QtCore.Qt.LeftButton:
                self.drag_start_pos = event.position().toPoint()
                return True

            if event.type() == QtCore.QEvent.MouseMove and self.drag_start_pos:
                dist = (event.position().toPoint() - self.drag_start_pos).manhattanLength()
                if dist < QtWidgets.QApplication.startDragDistance():
                    return True

                drag = QtGui.QDrag(self)
                mime = QtCore.QMimeData()
                mime.setText(self.label.text())
                drag.setMimeData(mime)

                pixmap = self.grab()
                drag.setPixmap(pixmap)
                drag.setHotSpot(event.position().toPoint() - self.rect().topLeft())
                drag.exec_(QtCore.Qt.MoveAction)

                self.drag_start_pos = None
                return True

            if event.type() == QtCore.QEvent.MouseButtonRelease:
                self.drag_start_pos = None
                return True

        return super().eventFilter(obj, event)


# ------------------------------------------------------------
# Group Widget (PRO CARD)
# ------------------------------------------------------------
class GroupWidget(QtWidgets.QFrame):
    def __init__(self, group_name, parent=None):
        super().__init__(parent)
        self.setObjectName("GroupCard")
        self.setAcceptDrops(True)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Maximum)

        # Shadow (optional)
        shadow = QtWidgets.QGraphicsDropShadowEffect(self)
        shadow.setBlurRadius(18)
        shadow.setOffset(0, 2)
        shadow.setColor(QtGui.QColor(0, 0, 0, 140))
        self.setGraphicsEffect(shadow)

        self.setStyleSheet("""
            #GroupCard {
                background: #2b2b2b;
                border: 1px solid #3a3a3a;
                border-radius: 10px;
            }
            #GroupHeader {
                background: #262626;
                border-top-left-radius: 10px;
                border-top-right-radius: 10px;
            }
            #GroupBody {
                background: #2b2b2b;
                border-bottom-left-radius: 10px;
                border-bottom-right-radius: 10px;
            }
            QLabel#GroupTitle {
                color: #e6e6e6;
                font-weight: 800;
                font-size: 12px;
            }
            QLabel#GroupSub {
                color: #a8a8a8;
                font-size: 10px;
            }
            QToolButton {
                background: transparent;
                border: none;
                color: #cfcfcf;
                padding: 2px 6px;
                border-radius: 4px;
            }
            QToolButton:hover {
                background: #3a3a3a;
                color: white;
            }
            QPushButton#AddLayerBtn {
                background: #3a3a3a;
                border: 1px solid #4a4a4a;
                border-radius: 6px;
                padding: 4px 10px;
                color: #eaeaea;
            }
            QPushButton#AddLayerBtn:hover { background: #4a4a4a; }
        """)

        self.layer_count = 0
        self._selected_layer = None
        self._collapsed = False

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Header
        self.header = QtWidgets.QFrame()
        self.header.setObjectName("GroupHeader")
        hl = QtWidgets.QHBoxLayout(self.header)
        hl.setContentsMargins(10, 8, 10, 8)
        hl.setSpacing(8)

        self.collapse_btn = QtWidgets.QToolButton()
        self.collapse_btn.setText("▾")
        self.collapse_btn.setToolTip("Collapse / Expand")
        self.collapse_btn.clicked.connect(self.toggle_collapsed)

        self.title_label = QtWidgets.QLabel(group_name)
        self.title_label.setObjectName("GroupTitle")

        self.sub_label = QtWidgets.QLabel("Modifiers")
        self.sub_label.setObjectName("GroupSub")

        # Group weight
        self.weight_label = QtWidgets.QLabel("Grp Wgt:")
        self.weight_label.setObjectName("GroupSub")

        self.group_slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.group_slider.setRange(0, 100)
        self.group_slider.setValue(100)
        self.group_slider.setFixedWidth(110)
        self.group_slider.setStyleSheet("""
            QSlider::groove:horizontal { height: 2px; background: #1e1e1e; }
            QSlider::sub-page:horizontal { background: #aa8844; }
            QSlider::handle:horizontal {
                background: #d0d0d0;
                width: 10px; height: 10px;
                margin: -4px 0;
                border-radius: 5px;
                border: 1px solid #555;
            }
        """)
        self.group_slider.valueChanged.connect(self.on_group_slider_change)

        self.group_value = QtWidgets.QLabel("1.00")
        self.group_value.setObjectName("GroupSub")
        self.group_value.setMinimumWidth(34)
        self.group_value.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        self.add_layer_btn = QtWidgets.QPushButton("+ Layer")
        self.add_layer_btn.setObjectName("AddLayerBtn")
        self.add_layer_btn.clicked.connect(self.add_layer)

        hl.addWidget(self.collapse_btn)
        hl.addWidget(self.title_label)
        hl.addWidget(self.sub_label)
        hl.addStretch()
        hl.addWidget(self.weight_label)
        hl.addWidget(self.group_slider)
        hl.addWidget(self.group_value)
        hl.addWidget(self.add_layer_btn)

        root.addWidget(self.header)

        # Divider
        divider = QtWidgets.QFrame()
        divider.setFixedHeight(1)
        divider.setStyleSheet("background:#3a3a3a;")
        root.addWidget(divider)

        # Body
        self.body = QtWidgets.QFrame()
        self.body.setObjectName("GroupBody")
        bl = QtWidgets.QVBoxLayout(self.body)
        bl.setContentsMargins(10, 10, 10, 10)
        bl.setSpacing(6)

        self.layers_layout = QtWidgets.QVBoxLayout()
        self.layers_layout.setSpacing(6)
        bl.addLayout(self.layers_layout)

        root.addWidget(self.body)

        # Drop indicator
        self.drop_indicator = QtWidgets.QFrame()
        self.drop_indicator.setFixedHeight(2)
        self.drop_indicator.setStyleSheet("background:#4aa3ff; border-radius:1px;")
        self.drop_indicator.hide()

        # Group context menu
        self.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self.show_context_menu)

        self.on_group_slider_change(self.group_slider.value())

    # -------- selection ----------
    def set_selected_layer(self, layer: LayerWidget):
        if self._selected_layer and self._selected_layer is not layer:
            self._selected_layer.set_selected(False)
        self._selected_layer = layer
        if self._selected_layer:
            self._selected_layer.set_selected(True)

    def clear_selection(self):
        if self._selected_layer:
            self._selected_layer.set_selected(False)
        self._selected_layer = None

    # -------- collapse ----------
    def toggle_collapsed(self):
        self._collapsed = not self._collapsed
        self.body.setVisible(not self._collapsed)
        self.collapse_btn.setText("▸" if self._collapsed else "▾")

    # -------- weights ----------
    def get_group_weight(self):
        return self.group_slider.value() / 100.0

    def on_group_slider_change(self, value):
        group_val = value / 100.0
        self.group_value.setText(f"{group_val:.2f}")
        self.group_slider.setToolTip(f"Group Weight: {group_val:.2f}")

        # refresh child readouts/tooltips
        for i in range(self.layers_layout.count()):
            w = self.layers_layout.itemAt(i).widget()
            if isinstance(w, LayerWidget):
                w.on_slider_change(w.slider.value())

    # -------- context menu ----------
    def show_context_menu(self, pos):
        menu = QtWidgets.QMenu(self)
        menu.addAction("Rename Group").triggered.connect(self.rename_group)
        menu.addAction("Delete Group").triggered.connect(self.delete_group)
        menu.exec_(self.mapToGlobal(pos))

    def rename_group(self):
        new_name, ok = QtWidgets.QInputDialog.getText(self, "Rename Group", "New Name:", text=self.title_label.text())
        if ok and new_name:
            self.title_label.setText(new_name)

    def delete_group(self):
        resp = QtWidgets.QMessageBox.question(
            self, "Delete Group",
            f"Delete group '{self.title_label.text()}' and all its layers?",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No
        )
        if resp != QtWidgets.QMessageBox.Yes:
            return
        self.setParent(None)
        self.deleteLater()

    # -------- layer lifecycle ----------
    def _wire_layer(self, layer: LayerWidget):
        # Safe disconnect in case moved between groups
        for sig in (layer.clicked, layer.request_delete, layer.request_rename):
            try:
                sig.disconnect()
            except:
                pass

        layer.clicked.connect(self.set_selected_layer)
        layer.request_delete.connect(self.delete_layer)
        layer.request_rename.connect(self.rename_layer)

        layer.on_slider_change(layer.slider.value())

    def add_layer(self):
        self.layer_count += 1
        layer_name = f"Layer_{self.layer_count:02d}"
        layer = LayerWidget(layer_name)
        self.layers_layout.insertWidget(0, layer)
        self._wire_layer(layer)

    def rename_layer(self, layer: LayerWidget):
        new_name, ok = QtWidgets.QInputDialog.getText(self, "Rename Layer", "New Name:", text=layer.label.text())
        if ok and new_name:
            layer.label.setText(new_name)

    def delete_layer(self, layer: LayerWidget):
        resp = QtWidgets.QMessageBox.question(
            self, "Delete Layer",
            f"Delete '{layer.label.text()}'?",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No
        )
        if resp != QtWidgets.QMessageBox.Yes:
            return
        if self._selected_layer is layer:
            self.clear_selection()
        layer.setParent(None)
        layer.deleteLater()

    # -------- drag & drop ----------
    def dragEnterEvent(self, event):
        if event.source() and isinstance(event.source(), LayerWidget):
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if event.source() and isinstance(event.source(), LayerWidget):
            # show indicator
            target_index = self._target_index_from_pos(event.position().toPoint())
            self._show_drop_indicator(target_index)
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragLeaveEvent(self, event):
        self._hide_drop_indicator()
        super().dragLeaveEvent(event)

    def dropEvent(self, event):
        source = event.source()
        if not source or not isinstance(source, LayerWidget):
            self._hide_drop_indicator()
            event.ignore()
            return

        target_index = self._target_index_from_pos(event.position().toPoint())
        self._hide_drop_indicator()

        QtCore.QTimer.singleShot(0, lambda: self.finalize_move_layer(source, target_index))
        event.acceptProposedAction()

    def _layer_widgets(self):
        out = []
        for i in range(self.layers_layout.count()):
            w = self.layers_layout.itemAt(i).widget()
            if isinstance(w, LayerWidget):
                out.append(w)
        return out

    def _target_index_from_pos(self, pos_in_group):
        # Convert to body-space (where layers live)
        pos = self.body.mapFrom(self, pos_in_group)

        layers = self._layer_widgets()
        for i, w in enumerate(layers):
            geo = w.geometry()
            if pos.y() < geo.y() + geo.height() / 2:
                return i
        return len(layers)

    def _show_drop_indicator(self, layer_index):
        self.layers_layout.removeWidget(self.drop_indicator)
        self.drop_indicator.show()
        self.layers_layout.insertWidget(layer_index, self.drop_indicator)

    def _hide_drop_indicator(self):
        self.layers_layout.removeWidget(self.drop_indicator)
        self.drop_indicator.hide()

    def finalize_move_layer(self, source_widget, target_index):
        # Robust cross-group move: remove from old layout if needed
        old_group = source_widget.parent()
        if isinstance(old_group, GroupWidget) and old_group is not self:
            old_group.layers_layout.removeWidget(source_widget)

        # Within same group adjust index if moving down
        current_layers = self._layer_widgets()
        if source_widget in current_layers:
            cur_index = current_layers.index(source_widget)
            if cur_index < target_index:
                target_index -= 1

        self.layers_layout.insertWidget(target_index, source_widget)
        self._wire_layer(source_widget)
        self.set_selected_layer(source_widget)


# ------------------------------------------------------------
# Main Window
# ------------------------------------------------------------
class LayerModifierTool(MayaQWidgetDockableMixin, QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Layer Modifiers (PySide6)")
        self.resize(380, 540)
        self.group_count = 0

        self.setStyleSheet("""
            QWidget { background: #3a3a3a; }
            QLineEdit {
                background:#242424;
                border:1px solid #2f2f2f;
                border-radius:6px;
                padding:6px 10px;
                color:#ddd;
            }
            QLineEdit:focus { border:1px solid #4aa3ff; }
            QPushButton#PrimaryBtn {
                background:#0077b6;
                border:1px solid #0b5f8a;
                color:white;
                font-weight:800;
                border-radius:8px;
                padding:6px 12px;
                min-height: 26px;
            }
            QPushButton#PrimaryBtn:hover { background:#008bd6; }
        """)

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(10)

        # Top bar
        top = QtWidgets.QHBoxLayout()
        top.setSpacing(10)

        self.add_group_btn = QtWidgets.QPushButton("+ Create Group")
        self.add_group_btn.setObjectName("PrimaryBtn")
        self.add_group_btn.clicked.connect(self.add_group)

        self.search = QtWidgets.QLineEdit()
        self.search.setPlaceholderText("Filter layers...")
        self.search.setClearButtonEnabled(True)
        self.search.textChanged.connect(self.apply_filter)

        top.addWidget(self.add_group_btn)
        top.addWidget(self.search, 1)
        root.addLayout(top)

        # Scroll area
        self.scroll_area = QtWidgets.QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setFrameShape(QtWidgets.QFrame.NoFrame)

        self.scroll_content = QtWidgets.QWidget()
        self.scroll_content.setStyleSheet("background:#3a3a3a;")
        self.scroll_layout = QtWidgets.QVBoxLayout(self.scroll_content)
        self.scroll_layout.setAlignment(QtCore.Qt.AlignTop)
        self.scroll_layout.setContentsMargins(4, 4, 4, 4)
        self.scroll_layout.setSpacing(12)

        self.scroll_area.setWidget(self.scroll_content)
        root.addWidget(self.scroll_area)

        # Shortcuts
        QtGui.QShortcut(QtGui.QKeySequence("F2"), self, activated=self.rename_selected)
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_Delete), self, activated=self.delete_selected)

    def groups(self):
        out = []
        for i in range(self.scroll_layout.count()):
            w = self.scroll_layout.itemAt(i).widget()
            if isinstance(w, GroupWidget):
                out.append(w)
        return out

    def add_group(self):
        self.group_count += 1
        group_name = f"Modifier Group {self.group_count}"
        group = GroupWidget(group_name)
        self.scroll_layout.addWidget(group)
        self.apply_filter(self.search.text())

    def _selected_layer(self):
        for g in self.groups():
            if g._selected_layer:
                return g, g._selected_layer
        return None, None

    def rename_selected(self):
        g, layer = self._selected_layer()
        if g and layer:
            g.rename_layer(layer)

    def delete_selected(self):
        g, layer = self._selected_layer()
        if g and layer:
            g.delete_layer(layer)

    def apply_filter(self, text):
        t = (text or "").strip().lower()
        for g in self.groups():
            visible_any = False
            for lw in g._layer_widgets():
                ok = (t in lw.label.text().lower()) if t else True
                lw.setVisible(ok)
                visible_any = visible_any or ok
            g.setVisible(visible_any if t else True)


# ------------------------------------------------------------
# Launch in Maya
# ------------------------------------------------------------
def show_ui():
    global layer_modifier_ui
    try:
        layer_modifier_ui.close()
        layer_modifier_ui.deleteLater()
    except:
        pass

    layer_modifier_ui = LayerModifierTool()
    layer_modifier_ui.show(dockable=True)


show_ui()
