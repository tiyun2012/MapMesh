import numpy as np


class Math3D:
    EPS = 1e-12

    @staticmethod
    def closest_point_on_segment(p, a, b):
        ab = b - a
        denom = np.dot(ab, ab)
        if denom <= Math3D.EPS:
            return a
        t = np.dot(p - a, ab) / denom
        t = np.clip(t, 0.0, 1.0)
        return a + t * ab

    @staticmethod
    def closest_point_on_degenerate_triangle(p, a, b, c):
        cp_ab = Math3D.closest_point_on_segment(p, a, b)
        cp_bc = Math3D.closest_point_on_segment(p, b, c)
        cp_ca = Math3D.closest_point_on_segment(p, c, a)

        d2_ab = np.dot(p - cp_ab, p - cp_ab)
        d2_bc = np.dot(p - cp_bc, p - cp_bc)
        d2_ca = np.dot(p - cp_ca, p - cp_ca)

        if d2_ab <= d2_bc and d2_ab <= d2_ca:
            return cp_ab
        if d2_bc <= d2_ca:
            return cp_bc
        return cp_ca

    @staticmethod
    def closest_point_on_triangle(p, a, b, c):
        ab = b - a
        ac = c - a
        normal = np.cross(ab, ac)
        if np.dot(normal, normal) <= Math3D.EPS:
            return Math3D.closest_point_on_degenerate_triangle(p, a, b, c)

        ap = p - a
        d1 = np.dot(ab, ap)
        d2 = np.dot(ac, ap)
        if d1 <= 0.0 and d2 <= 0.0:
            return a

        bp = p - b
        d3 = np.dot(ab, bp)
        d4 = np.dot(ac, bp)
        if d3 >= 0.0 and d4 <= d3:
            return b

        vc = d1 * d4 - d3 * d2
        if vc <= 0.0 and d1 >= 0.0 and d3 <= 0.0:
            v = d1 / (d1 - d3 + Math3D.EPS)
            return a + v * ab

        cp = p - c
        d5 = np.dot(ab, cp)
        d6 = np.dot(ac, cp)
        if d6 >= 0.0 and d5 <= d6:
            return c

        vb = d5 * d2 - d1 * d6
        if vb <= 0.0 and d2 >= 0.0 and d6 <= 0.0:
            w = d2 / (d2 - d6 + Math3D.EPS)
            return a + w * ac

        va = d3 * d6 - d5 * d4
        if va <= 0.0 and (d4 - d3) >= 0.0 and (d5 - d6) >= 0.0:
            w = (d4 - d3) / ((d4 - d3) + (d5 - d6) + Math3D.EPS)
            return b + w * (c - b)

        denom = 1.0 / (va + vb + vc + Math3D.EPS)
        v = vb * denom
        w = vc * denom
        return a + ab * v + ac * w


def point_aabb_distance_sq(p, min_bound, max_bound):
    dist_low = np.maximum(0.0, min_bound - p)
    dist_high = np.maximum(0.0, p - max_bound)
    return float(np.sum(dist_low * dist_low + dist_high * dist_high))