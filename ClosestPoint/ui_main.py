import time
import numpy as np
import pyqtgraph.opengl as gl
from PySide6.QtWidgets import (
    QMainWindow,
    QVBoxLayout,
    QHBoxLayout,
    QWidget,
    QSlider,
    QLabel,
    QPushButton,
    QFileDialog,
)
from PySide6.QtCore import Qt

from engine_bvh import AABBNode, find_closest_point
from ui_widgets import MayaViewWidget


class ProfessionalClosestPointApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Geometry Engine v1.0")
        self.resize(1400, 900)

        self.vertices = None
        self.faces = None
        self.tree = None
        self.debug_boxes = []

        self.init_ui()
        self.build_mesh_data(*self.generate_torus())

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QHBoxLayout(central_widget)

        sidebar = QVBoxLayout()
        sidebar.setContentsMargins(10, 10, 10, 10)

        title = QLabel("BVH CONTROLS")
        title.setStyleSheet("font-weight: bold; font-size: 16px; color: #55aaff;")
        sidebar.addWidget(title)

        btn_load = QPushButton("Load OBJ Mesh")
        btn_load.clicked.connect(self.on_load_mesh)
        btn_load.setStyleSheet("padding: 10px; background: #333; color: white;")
        sidebar.addWidget(btn_load)

        sidebar.addSpacing(20)

        self.sld_x = self.create_slider(sidebar, "Query X", -100, 100, 20)
        self.sld_y = self.create_slider(sidebar, "Query Y", -100, 100, 0)
        self.sld_z = self.create_slider(sidebar, "Query Z", -100, 100, 0)

        sidebar.addSpacing(20)

        self.btn_debug = QPushButton("Toggle BVH Visualizer")
        self.btn_debug.setCheckable(True)
        self.btn_debug.clicked.connect(self.update_debug_view)
        sidebar.addWidget(self.btn_debug)

        self.info_label = QLabel("Ready.")
        self.info_label.setStyleSheet("font-family: monospace; background: #222; padding: 10px;")
        sidebar.addWidget(self.info_label)

        sidebar.addStretch()
        layout.addLayout(sidebar, 1)

        self.view = MayaViewWidget()
        self.view.setBackgroundColor("#111")
        self.view.setCameraPosition(distance=15)
        layout.addWidget(self.view, 4)

        self.mesh_item = None
        self.query_radius = 0.2
        self.result_radius = 0.15
        self.query_sphere = self.create_sphere_item(self.query_radius, (1, 0, 0, 1))
        self.result_sphere = self.create_sphere_item(self.result_radius, (0, 1, 0, 1))
        self.line = gl.GLLinePlotItem(color=(1, 1, 1, 1), width=2)

        self.view.addItem(self.query_sphere)
        self.view.addItem(self.result_sphere)
        self.view.addItem(self.line)

    def create_slider(self, layout, name, min_v, max_v, def_v):
        layout.addWidget(QLabel(name))
        sld = QSlider(Qt.Horizontal)
        sld.setRange(min_v, max_v)
        sld.setValue(def_v)
        sld.valueChanged.connect(self.update_query)
        layout.addWidget(sld)
        return sld

    def create_sphere_item(self, radius, color):
        mesh = gl.MeshData.sphere(rows=8, cols=16)
        item = gl.GLMeshItem(meshdata=mesh, smooth=True, color=color, shader="shaded")
        item.scale(radius, radius, radius)
        return item

    def place_sphere(self, item, radius, x, y, z):
        item.resetTransform()
        item.scale(radius, radius, radius)
        item.translate(x, y, z)

    def on_load_mesh(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open Mesh", "", "Mesh Files (*.obj)")
        if path:
            verts, faces = self.parse_obj(path)
            if len(verts) == 0 or len(faces) == 0:
                self.info_label.setText("Failed to load mesh: no vertices or faces found.")
                return
            self.build_mesh_data(verts, faces)

    def parse_obj(self, filename):
        verts = []
        faces = []
        with open(filename, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("v "):
                    parts = line.split()
                    if len(parts) >= 4:
                        verts.append([float(parts[1]), float(parts[2]), float(parts[3])])
                elif line.startswith("f "):
                    parts = line.split()[1:]
                    idx = []
                    for p in parts:
                        if not p:
                            continue
                        raw = p.split("/")[0]
                        if not raw:
                            continue
                        vi = int(raw)
                        if vi < 0:
                            vi = len(verts) + vi
                        else:
                            vi = vi - 1
                        idx.append(vi)
                    if len(idx) >= 3:
                        base = idx[0]
                        for i in range(1, len(idx) - 1):
                            faces.append([base, idx[i], idx[i + 1]])
        return np.array(verts, dtype=np.float32), np.array(faces, dtype=np.int32)

    def build_mesh_data(self, verts, faces):
        self.vertices = verts
        self.faces = faces

        start = time.time()
        self.tree = AABBNode(
            np.arange(len(faces)),
            verts,
            faces,
            split_method="sah",
            leaf_size=8,
            max_depth=32,
        )
        end = time.time()

        if self.mesh_item:
            self.view.removeItem(self.mesh_item)

        mdata = gl.MeshData(vertexes=verts, faces=faces)
        self.mesh_item = gl.GLMeshItem(
            meshdata=mdata,
            smooth=True,
            color=(0.2, 0.4, 0.8, 0.3),
            shader="shaded",
            glOptions="additive",
        )
        self.view.addItem(self.mesh_item)

        self.info_label.setText(f"BVH Built: {len(faces)} faces\nTime: {(end - start) * 1000:.2f}ms")
        self.update_debug_view()
        self.update_query()

    def update_debug_view(self):
        for box in self.debug_boxes:
            self.view.removeItem(box)
        self.debug_boxes = []

        if self.btn_debug.isChecked() and self.tree:
            self.draw_node_bounds(self.tree, 0)

    def draw_node_bounds(self, node, depth):
        if depth > 3:
            return

        mi, ma = node.min_bound, node.max_bound
        pts = np.array(
            [
                [mi[0], mi[1], mi[2]],
                [ma[0], mi[1], mi[2]],
                [ma[0], ma[1], mi[2]],
                [mi[0], ma[1], mi[2]],
                [mi[0], mi[1], ma[2]],
                [ma[0], mi[1], ma[2]],
                [ma[0], ma[1], ma[2]],
                [mi[0], ma[1], ma[2]],
            ],
            dtype=np.float32,
        )
        edges = np.array(
            [
                [0, 1],
                [1, 2],
                [2, 3],
                [3, 0],
                [4, 5],
                [5, 6],
                [6, 7],
                [7, 4],
                [0, 4],
                [1, 5],
                [2, 6],
                [3, 7],
            ],
            dtype=np.int32,
        )

        for e in edges:
            line = gl.GLLinePlotItem(pos=pts[e], color=(1, 1, 0, 0.2), width=1, antialias=True)
            self.view.addItem(line)
            self.debug_boxes.append(line)

        if node.left:
            self.draw_node_bounds(node.left, depth + 1)
        if node.right:
            self.draw_node_bounds(node.right, depth + 1)

    def generate_torus(self, r1=3.0, r2=1.0, segments=32):
        phi = np.linspace(0, 2 * np.pi, segments, endpoint=False)
        theta = np.linspace(0, 2 * np.pi, segments, endpoint=False)
        phi, theta = np.meshgrid(phi, theta)
        x = (r1 + r2 * np.cos(theta)) * np.cos(phi)
        y = (r1 + r2 * np.cos(theta)) * np.sin(phi)
        z = r2 * np.sin(theta)
        pts = np.stack([x.flatten(), y.flatten(), z.flatten()], axis=1).astype(np.float32)
        faces = []
        for i in range(segments):
            for j in range(segments):
                i_n, j_n = (i + 1) % segments, (j + 1) % segments
                p1, p2, p3, p4 = i * segments + j, i * segments + j_n, i_n * segments + j, i_n * segments + j_n
                faces.append([p1, p2, p3])
                faces.append([p2, p4, p3])
        return pts, np.array(faces, dtype=np.int32)

    def update_query(self):
        if self.vertices is None or self.tree is None:
            return

        qx = self.sld_x.value() / 5.0
        qy = self.sld_y.value() / 5.0
        qz = self.sld_z.value() / 5.0
        query_pos = np.array([qx, qy, qz], dtype=np.float32)

        self.place_sphere(self.query_sphere, self.query_radius, qx, qy, qz)

        best = {"dist_sq": float("inf"), "point": np.array([0.0, 0.0, 0.0], dtype=np.float32)}
        find_closest_point(query_pos, self.tree, self.vertices, self.faces, best)

        res = best["point"]
        self.place_sphere(self.result_sphere, self.result_radius, float(res[0]), float(res[1]), float(res[2]))
        self.line.setData(pos=np.array([query_pos, res], dtype=np.float32))

        self.info_label.setText(f"Dist: {np.sqrt(best['dist_sq']):.4f}\nPos: {res}")