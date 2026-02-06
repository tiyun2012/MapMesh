# 3D Closest Point (PySide6 + pyqtgraph)

This is a runnable Python app that builds an AABB tree over a mesh and finds the closest point in real time as you move a query point.

## Setup

```bash
pip install -r requirements.txt
```

## Run

```bash
python app.py
```

## Controls

- Use the X/Y/Z sliders to move the red query sphere.
- The green sphere is the closest point on the mesh.
- The white line shows the shortest distance.