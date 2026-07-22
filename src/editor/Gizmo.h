#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include "renderer/Mesh.h"

// Which axis is being dragged
enum class GizmoAxis { None, X, Y, Z };

// Which gizmo operation mode
enum class GizmoMode { Translate, Rotate, Scale };

struct GizmoState {
    GizmoAxis  activeAxis   = GizmoAxis::None;
    GizmoAxis  hoveredAxis  = GizmoAxis::None;
    GizmoMode  mode         = GizmoMode::Translate;
    bool       dragging     = false;
    glm::vec3  dragStart    = glm::vec3(0.0f);
    glm::vec3  dragOrigin   = glm::vec3(0.0f); // node position at drag start
    glm::vec3  dragScaleOrigin = glm::vec3(1.0f);
    glm::vec3  dragRotationOrigin = glm::vec3(0.0f);
    glm::vec2  dragStartMouse = glm::vec2(0.0f);
};

// Ray from camera through mouse pixel
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

Ray buildRay(float mouseX, float mouseY,
             float screenW, float screenH,
             const glm::mat4& view,
             const glm::mat4& proj);

// Returns t along ray for closest point to axis line, or -1 if no hit
float rayAxisHit(const Ray& ray,
                 const glm::vec3& nodePos,
                 GizmoAxis axis,
                 float hitRadius = 0.08f);

float rayCircleHit(const Ray& ray,
                  const glm::vec3& center,
                  GizmoAxis axis,
                  float radius = 0.85f,
                  float thickness = 0.06f);

// Returns the closest point on the gizmo axis line in world space
bool rayAxisClosestPoint(const Ray& ray,
                         const glm::vec3& nodePos,
                         GizmoAxis axis,
                         glm::vec3* outPoint,
                         float hitRadius = 0.08f);

bool rayCircleClosestPoint(const Ray& ray,
                           const glm::vec3& center,
                           GizmoAxis axis,
                           glm::vec3* outPoint,
                           float radius = 0.85f,
                           float thickness = 0.06f);

// Intersects a ray with a plane defined by a point and normal
bool rayPlaneIntersect(const Ray& ray,
                       const glm::vec3& planePoint,
                       const glm::vec3& planeNormal,
                       glm::vec3* outPoint);

// Build arrow geometry for one axis
// shaft = cylinder approximated as line segments, head = cone
std::vector<GizmoVertex> buildArrow(glm::vec3 dir, glm::vec3 color,
                                    int segments = 8);

// Build full gizmo centered at origin for the selected operation mode
std::vector<GizmoVertex> buildGizmoGeometry(GizmoMode mode);