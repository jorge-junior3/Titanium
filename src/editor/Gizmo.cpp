#include "Gizmo.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>
#include <array>

// ============================================================
// Ray building
// ============================================================

Ray buildRay(float mouseX, float mouseY,
             float screenW, float screenH,
             const glm::mat4& view,
             const glm::mat4& proj) {
    // NDC
    float ndcX = (2.0f * mouseX) / screenW - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / screenH; // flip Y

    glm::vec4 clipRay  = glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 eyeRay   = glm::inverse(proj) * clipRay;
    eyeRay             = glm::vec4(eyeRay.x, eyeRay.y, -1.0f, 0.0f);
    glm::vec3 worldRay = glm::normalize(glm::vec3(glm::inverse(view) * eyeRay));
    glm::vec3 origin   = glm::vec3(glm::inverse(view) * glm::vec4(0, 0, 0, 1));

    return {origin, worldRay};
}

// ============================================================
// Ray vs infinite axis line — returns closest t or -1
// ============================================================

static glm::vec3 axisDirection(GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return glm::vec3(1, 0, 0);
        case GizmoAxis::Y: return glm::vec3(0, 1, 0);
        case GizmoAxis::Z: return glm::vec3(0, 0, 1);
        default: return glm::vec3(0.0f);
    }
}

float rayAxisHit(const Ray& ray, const glm::vec3& nodePos,
                 GizmoAxis axis, float hitRadius) {
    glm::vec3 axisDir = axisDirection(axis);
    if (glm::dot(axisDir, axisDir) < 1e-6f) return -1.0f;

    // closest distance between two lines (ray and axis line)
    glm::vec3 w0   = ray.origin - nodePos;
    float     a    = glm::dot(ray.direction, ray.direction);
    float     b    = glm::dot(ray.direction, axisDir);
    float     c    = glm::dot(axisDir, axisDir);
    float     d    = glm::dot(ray.direction, w0);
    float     e    = glm::dot(axisDir, w0);
    float     denom = a * c - b * b;

    if (std::abs(denom) < 1e-6f) return -1.0f;

    float t  = (b * e - c * d) / denom;
    float s  = (a * e - b * d) / denom;

    // only hit if along the arrow shaft (0 to 1.2 units)
    if (s < -0.2f || s > 1.2f || t < 0.0f) return -1.0f;

    glm::vec3 closestOnRay  = ray.origin    + t * ray.direction;
    glm::vec3 closestOnAxis = nodePos       + s * axisDir;
    float     dist          = glm::length(closestOnRay - closestOnAxis);

    return dist < hitRadius ? t : -1.0f;
}

bool rayAxisClosestPoint(const Ray& ray,
                         const glm::vec3& nodePos,
                         GizmoAxis axis,
                         glm::vec3* outPoint,
                         float hitRadius) {
    glm::vec3 axisDir = axisDirection(axis);
    if (glm::dot(axisDir, axisDir) < 1e-6f) return false;

    glm::vec3 w0 = ray.origin - nodePos;
    float a = glm::dot(ray.direction, ray.direction);
    float b = glm::dot(ray.direction, axisDir);
    float c = glm::dot(axisDir, axisDir);
    float d = glm::dot(ray.direction, w0);
    float e = glm::dot(axisDir, w0);
    float denom = a * c - b * b;

    if (std::abs(denom) < 1e-6f) return false;

    float t = (b * e - c * d) / denom;
    float s = (a * e - b * d) / denom;
    if (s < -0.2f || s > 1.2f || t < 0.0f) return false;

    glm::vec3 closestOnRay  = ray.origin + t * ray.direction;
    glm::vec3 closestOnAxis = nodePos + s * axisDir;
    float dist = glm::length(closestOnRay - closestOnAxis);

    if (dist < hitRadius && outPoint) {
        *outPoint = closestOnAxis;
        return true;
    }

    return false;
}

bool rayPlaneIntersect(const Ray& ray,
                       const glm::vec3& planePoint,
                       const glm::vec3& planeNormal,
                       glm::vec3* outPoint) {
    float denom = glm::dot(planeNormal, ray.direction);
    if (std::abs(denom) < 1e-6f) return false;

    float t = glm::dot(planeNormal, planePoint - ray.origin) / denom;
    if (t < 0.0f) return false;

    if (outPoint) *outPoint = ray.origin + t * ray.direction;
    return true;
}

static glm::vec3 projectOntoPlane(const glm::vec3& v, const glm::vec3& normal) {
    return v - normal * glm::dot(v, normal);
}

float rayCircleHit(const Ray& ray,
                   const glm::vec3& center,
                   GizmoAxis axis,
                   float radius,
                   float thickness) {
    glm::vec3 axisDir = axisDirection(axis);
    glm::vec3 hitPoint;
    if (!rayPlaneIntersect(ray, center, axisDir, &hitPoint)) return -1.0f;

    float t = glm::dot(hitPoint - ray.origin, ray.direction);
    if (t < 0.0f) return -1.0f;

    glm::vec3 diff = hitPoint - center;
    glm::vec3 planeVec = projectOntoPlane(diff, axisDir);
    float dist = glm::length(planeVec);

    if (dist < radius - thickness || dist > radius + thickness)
        return -1.0f;

    return t;
}

bool rayCircleClosestPoint(const Ray& ray,
                           const glm::vec3& center,
                           GizmoAxis axis,
                           glm::vec3* outPoint,
                           float radius,
                           float thickness) {
    glm::vec3 axisDir = axisDirection(axis);
    glm::vec3 hitPoint;
    if (!rayPlaneIntersect(ray, center, axisDir, &hitPoint)) return false;

    glm::vec3 diff = hitPoint - center;
    glm::vec3 planeVec = projectOntoPlane(diff, axisDir);
    float len = glm::length(planeVec);
    if (len < 1e-6f) return false;

    glm::vec3 circlePoint = center + planeVec / len * radius;
    if (outPoint) *outPoint = circlePoint;
    return true;
}

static void addOrientedBox(std::vector<GizmoVertex>& verts,
                           const glm::vec3& center,
                           const glm::vec3& axis,
                           float length,
                           float size,
                           const glm::vec3& color) {
    glm::vec3 dir = glm::normalize(axis);
    glm::vec3 ref = std::abs(dir.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 u   = glm::normalize(glm::cross(dir, ref));
    glm::vec3 v   = glm::cross(dir, u);
    glm::vec3 halfExtents = (u + v) * (size * 0.5f);
    glm::vec3 halfLength = dir * (length * 0.5f);

    std::array<glm::vec3, 8> corners = {
        center + halfLength + halfExtents,
        center + halfLength - halfExtents,
        center - halfLength - halfExtents,
        center - halfLength + halfExtents,
        center + halfLength + (u - v) * (size * 0.5f),
        center + halfLength - (u - v) * (size * 0.5f),
        center - halfLength - (u - v) * (size * 0.5f),
        center - halfLength + (u - v) * (size * 0.5f)
    };

    const std::array<std::array<int, 3>, 12> faces = {{
        {0, 1, 2}, {0, 2, 3},
        {4, 6, 5}, {4, 7, 6},
        {0, 4, 5}, {0, 5, 1},
        {1, 5, 6}, {1, 6, 2},
        {2, 6, 7}, {2, 7, 3},
        {3, 7, 4}, {3, 4, 0}
    }};

    for (const auto& face : faces) {
        verts.push_back({corners[face[0]], color});
        verts.push_back({corners[face[1]], color});
        verts.push_back({corners[face[2]], color});
    }
}

// ============================================================
// Arrow geometry — shaft + cone head
// ============================================================

std::vector<GizmoVertex> buildArrow(glm::vec3 dir, glm::vec3 color, int segments) {
    std::vector<GizmoVertex> verts;

    // perpendicular basis
    glm::vec3 up   = std::abs(dir.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 perp = glm::normalize(glm::cross(dir, up));
    glm::vec3 perp2 = glm::cross(dir, perp);

    float shaftLen    = 0.8f;
    float shaftRadius = 0.025f;
    float headLen     = 0.2f;
    float headRadius  = 0.06f;

    // shaft — cylinder as triangle strip pairs
    for (int i = 0; i <= segments; i++) {
        float angle  = (float)i / segments * glm::two_pi<float>();
        float cx     = std::cos(angle);
        float cy     = std::sin(angle);
        glm::vec3 offset = (perp * cx + perp2 * cy) * shaftRadius;

        glm::vec3 bottom = offset;
        glm::vec3 top    = dir * shaftLen + offset;

        int next = (i + 1) % segments;
        float na = (float)next / segments * glm::two_pi<float>();
        glm::vec3 noffset = (perp * std::cos(na) + perp2 * std::sin(na)) * shaftRadius;

        // two triangles per quad
        verts.push_back({bottom,           color});
        verts.push_back({top,              color});
        verts.push_back({noffset,          color});
        verts.push_back({top,              color});
        verts.push_back({dir * shaftLen + noffset, color});
        verts.push_back({noffset,          color});
    }

    // cone head
    glm::vec3 tip = dir * (shaftLen + headLen);
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i       / segments * glm::two_pi<float>();
        float a1 = (float)(i + 1) / segments * glm::two_pi<float>();

        glm::vec3 r0 = dir * shaftLen + (perp * std::cos(a0) + perp2 * std::sin(a0)) * headRadius;
        glm::vec3 r1 = dir * shaftLen + (perp * std::cos(a1) + perp2 * std::sin(a1)) * headRadius;

        // side face
        verts.push_back({r0,  color});
        verts.push_back({tip, color});
        verts.push_back({r1,  color});

        // base cap
        verts.push_back({dir * shaftLen, color});
        verts.push_back({r0,             color});
        verts.push_back({r1,             color});
    }

    return verts;
}

static void addBox(std::vector<GizmoVertex>& verts, const glm::vec3& center, float size, const glm::vec3& color) {
    const float h = size * 0.5f;
    const std::array<glm::vec3, 8> corners = {
        glm::vec3(-h, -h, -h), glm::vec3(h, -h, -h),
        glm::vec3(h, h, -h), glm::vec3(-h, h, -h),
        glm::vec3(-h, -h, h), glm::vec3(h, -h, h),
        glm::vec3(h, h, h), glm::vec3(-h, h, h)
    };

    const std::array<std::array<int, 3>, 12> faces = {{
        {0, 1, 2}, {0, 2, 3},
        {4, 6, 5}, {4, 7, 6},
        {0, 4, 5}, {0, 5, 1},
        {1, 5, 6}, {1, 6, 2},
        {2, 6, 7}, {2, 7, 3},
        {3, 7, 4}, {3, 4, 0}
    }};

    for (const auto& face : faces) {
        verts.push_back({center + corners[face[0]], color});
        verts.push_back({center + corners[face[1]], color});
        verts.push_back({center + corners[face[2]], color});
    }
}

static std::vector<GizmoVertex> buildRotationGizmo(glm::vec3 axis, glm::vec3 color, int segments = 64) {
    std::vector<GizmoVertex> verts;
    glm::vec3 ref = std::abs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 u = glm::normalize(glm::cross(axis, ref));
    glm::vec3 v = glm::cross(axis, u);

    const float innerRadius = 0.78f;
    const float outerRadius = 0.86f;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * glm::two_pi<float>();
        float a1 = (float)(i + 1) / segments * glm::two_pi<float>();
        glm::vec3 p0 = u * std::cos(a0) * innerRadius + v * glm::sin(a0) * innerRadius;
        glm::vec3 p1 = u * std::cos(a1) * innerRadius + v * glm::sin(a1) * innerRadius;
        glm::vec3 q0 = u * std::cos(a0) * outerRadius + v * glm::sin(a0) * outerRadius;
        glm::vec3 q1 = u * std::cos(a1) * outerRadius + v * glm::sin(a1) * outerRadius;

        verts.push_back({p0, color});
        verts.push_back({q0, color});
        verts.push_back({q1, color});

        verts.push_back({p0, color});
        verts.push_back({q1, color});
        verts.push_back({p1, color});
    }

    return verts;
}

static std::vector<GizmoVertex> buildBoxHandle(glm::vec3 dir, glm::vec3 color, float length, float boxSize) {
    std::vector<GizmoVertex> verts;
    glm::vec3 end = dir * length;
    verts.push_back({glm::vec3(0.0f), color});
    verts.push_back({end, color});
    addBox(verts, end, boxSize, color);
    return verts;
}

static std::vector<GizmoVertex> buildScaleGizmo(glm::vec3 dir, glm::vec3 color) {
    std::vector<GizmoVertex> verts;

    glm::vec3 up   = std::abs(dir.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 perp = glm::normalize(glm::cross(dir, up));
    glm::vec3 perp2 = glm::cross(dir, perp);

    float shaftLen    = 0.75f;
    float shaftRadius = 0.02f;
    float headOffset  = 0.85f;
    float headSize    = 0.14f;
    int   segments    = 12;

    for (int i = 0; i <= segments; i++) {
        float angle  = (float)i / segments * glm::two_pi<float>();
        float cx     = std::cos(angle);
        float cy     = std::sin(angle);
        glm::vec3 offset = (perp * cx + perp2 * cy) * shaftRadius;

        glm::vec3 bottom = offset;
        glm::vec3 top    = dir * shaftLen + offset;

        int next = (i + 1) % segments;
        float na = (float)next / segments * glm::two_pi<float>();
        glm::vec3 noffset = (perp * std::cos(na) + perp2 * std::sin(na)) * shaftRadius;

        verts.push_back({bottom,           color});
        verts.push_back({top,              color});
        verts.push_back({noffset,          color});
        verts.push_back({top,              color});
        verts.push_back({dir * shaftLen + noffset, color});
        verts.push_back({noffset,          color});
    }

    addBox(verts, dir * headOffset, headSize, color);
    return verts;
}

std::vector<GizmoVertex> buildGizmoGeometry(GizmoMode mode) {
    std::vector<GizmoVertex> verts;
    switch (mode) {
        case GizmoMode::Translate: {
            auto x = buildArrow({1,0,0}, {1.0f, 0.15f, 0.15f});
            auto y = buildArrow({0,1,0}, {0.15f, 1.0f, 0.15f});
            auto z = buildArrow({0,0,1}, {0.15f, 0.15f, 1.0f});
            x.insert(x.end(), y.begin(), y.end());
            x.insert(x.end(), z.begin(), z.end());
            return x;
        }
        case GizmoMode::Rotate: {
            auto x = buildRotationGizmo({1,0,0}, {1.0f, 0.15f, 0.15f});
            auto y = buildRotationGizmo({0,1,0}, {0.15f, 1.0f, 0.15f});
            auto z = buildRotationGizmo({0,0,1}, {0.15f, 0.15f, 1.0f});
            x.insert(x.end(), y.begin(), y.end());
            x.insert(x.end(), z.begin(), z.end());
            return x;
        }
        case GizmoMode::Scale: {
            auto x = buildScaleGizmo({1,0,0}, {1.0f, 0.15f, 0.15f});
            auto y = buildScaleGizmo({0,1,0}, {0.15f, 1.0f, 0.15f});
            auto z = buildScaleGizmo({0,0,1}, {0.15f, 0.15f, 1.0f});
            x.insert(x.end(), y.begin(), y.end());
            x.insert(x.end(), z.begin(), z.end());
            return x;
        }
    }

    return verts;
}