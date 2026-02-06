import numpy as np
from engine_math import Math3D, point_aabb_distance_sq


class AABBNode:
    def __init__(
        self,
        face_indices,
        vertices,
        faces,
        leaf_size=8,
        split_method="sah",
        max_depth=32,
    ):
        self.left = None
        self.right = None
        self.faces = face_indices

        tri = vertices[faces[face_indices]]
        self.min_bound = np.min(tri, axis=(0, 1))
        self.max_bound = np.max(tri, axis=(0, 1))

        if len(face_indices) > leaf_size and max_depth > 0:
            left_indices, right_indices = None, None
            if split_method == "sah":
                left_indices, right_indices = self._split_sah(face_indices, vertices, faces)

            if left_indices is None or right_indices is None:
                extent = self.max_bound - self.min_bound
                axis = int(np.argmax(extent))
                centroids = np.mean(tri, axis=1)
                median = np.median(centroids[:, axis])
                left_mask = centroids[:, axis] <= median
                right_mask = ~left_mask
                if np.any(left_mask) and np.any(right_mask):
                    left_indices = face_indices[left_mask]
                    right_indices = face_indices[right_mask]

            if left_indices is not None and right_indices is not None:
                self.left = AABBNode(
                    left_indices,
                    vertices,
                    faces,
                    leaf_size=leaf_size,
                    split_method=split_method,
                    max_depth=max_depth - 1,
                )
                self.right = AABBNode(
                    right_indices,
                    vertices,
                    faces,
                    leaf_size=leaf_size,
                    split_method=split_method,
                    max_depth=max_depth - 1,
                )
                self.faces = None

    @staticmethod
    def _surface_area(min_bound, max_bound):
        ext = max_bound - min_bound
        return 2.0 * (ext[..., 0] * ext[..., 1] + ext[..., 1] * ext[..., 2] + ext[..., 2] * ext[..., 0])

    def _split_sah(self, face_indices, vertices, faces):
        count = len(face_indices)
        if count <= 2:
            return None, None

        tri = vertices[faces[face_indices]]
        tri_min = np.min(tri, axis=1)
        tri_max = np.max(tri, axis=1)
        centroids = np.mean(tri, axis=1)

        best_cost = float("inf")
        best_axis = None
        best_split = None
        best_order = None

        for axis in range(3):
            order = np.argsort(centroids[:, axis])
            ordered_min = tri_min[order]
            ordered_max = tri_max[order]

            prefix_min = np.minimum.accumulate(ordered_min, axis=0)
            prefix_max = np.maximum.accumulate(ordered_max, axis=0)
            suffix_min = np.minimum.accumulate(ordered_min[::-1], axis=0)[::-1]
            suffix_max = np.maximum.accumulate(ordered_max[::-1], axis=0)[::-1]

            left_area = self._surface_area(prefix_min[:-1], prefix_max[:-1])
            right_area = self._surface_area(suffix_min[1:], suffix_max[1:])
            left_count = np.arange(1, count)
            right_count = count - left_count
            cost = left_area * left_count + right_area * right_count

            split_idx = int(np.argmin(cost))
            split_cost = float(cost[split_idx])
            if split_cost < best_cost:
                best_cost = split_cost
                best_axis = axis
                best_split = split_idx + 1
                best_order = order

        leaf_cost = float(self._surface_area(self.min_bound, self.max_bound) * count)
        if best_axis is None or best_cost >= leaf_cost * 0.95:
            return None, None

        left_indices = face_indices[best_order[:best_split]]
        right_indices = face_indices[best_order[best_split:]]
        if len(left_indices) == 0 or len(right_indices) == 0:
            return None, None
        return left_indices, right_indices


def find_closest_point(p, node, vertices, faces, best):
    box_dist_sq = point_aabb_distance_sq(p, node.min_bound, node.max_bound)
    if box_dist_sq >= best["dist_sq"]:
        return

    if node.left is None:
        for f_idx in node.faces:
            tri = vertices[faces[f_idx]]
            cp = Math3D.closest_point_on_triangle(p, tri[0], tri[1], tri[2])
            d2 = np.dot(p - cp, p - cp)
            if d2 < best["dist_sq"]:
                best["dist_sq"] = d2
                best["point"] = cp
    else:
        dist_l = point_aabb_distance_sq(p, node.left.min_bound, node.left.max_bound)
        dist_r = point_aabb_distance_sq(p, node.right.min_bound, node.right.max_bound)

        if dist_l < dist_r:
            find_closest_point(p, node.left, vertices, faces, best)
            if dist_r < best["dist_sq"]:
                find_closest_point(p, node.right, vertices, faces, best)
        else:
            find_closest_point(p, node.right, vertices, faces, best)
            if dist_l < best["dist_sq"]:
                find_closest_point(p, node.left, vertices, faces, best)