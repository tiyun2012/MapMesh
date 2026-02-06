import pyqtgraph.opengl as gl
from PySide6.QtCore import Qt


class MayaViewWidget(gl.GLViewWidget):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._last_pos = None

    def mousePressEvent(self, ev):
        self._last_pos = ev.position()
        super().mousePressEvent(ev)

    def mouseReleaseEvent(self, ev):
        self._last_pos = None
        super().mouseReleaseEvent(ev)

    def mouseMoveEvent(self, ev):
        if self._last_pos is None:
            self._last_pos = ev.position()
            super().mouseMoveEvent(ev)
            return

        pos = ev.position()
        dx = pos.x() - self._last_pos.x()
        dy = pos.y() - self._last_pos.y()
        self._last_pos = pos

        if ev.modifiers() & Qt.AltModifier:
            if ev.buttons() & Qt.LeftButton:
                self.orbit(-dx * 0.5, -dy * 0.5)
                ev.accept()
                return
            if ev.buttons() & Qt.MiddleButton:
                self.pan(dx, -dy, 0, relative="view")
                ev.accept()
                return
            if ev.buttons() & Qt.RightButton:
                scale = 0.999 ** (dy * 4.0)
                self.opts["distance"] = max(0.01, self.opts["distance"] * scale)
                self.update()
                ev.accept()
                return

        super().mouseMoveEvent(ev)

    def wheelEvent(self, ev):
        delta = ev.angleDelta().y()
        if delta == 0:
            delta = ev.angleDelta().x()
        scale = 0.999 ** delta
        self.opts["distance"] = max(0.01, self.opts["distance"] * scale)
        self.update()
        ev.accept()