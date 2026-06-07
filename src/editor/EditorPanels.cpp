#include "EditorPanels.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <cstring>

namespace editor {

static int g_selectedNode = -1;
static bool g_selectedAtmosphere = false;
static int g_lastSelectedNode = -1;
static bool g_lastSelectedAtmosphere = false;
static char g_nameBuffer[128] = {};
static glm::vec3 g_inspectorPosition = glm::vec3(0.0f);
static glm::vec3 g_inspectorRotation = glm::vec3(0.0f);
static glm::vec3 g_inspectorScale    = glm::vec3(1.0f);
static glm::vec3 g_inspectorColor    = glm::vec3(0.8f, 0.8f, 0.8f);
static Renderer::AtmosphereSettings g_inspectorAtmosphere;

static void syncInspectorState(const Renderer& renderer) {
    if (g_selectedAtmosphere) {
        g_nameBuffer[0] = '\0';
        g_lastSelectedAtmosphere = true;
        g_lastSelectedNode = -1;
        g_inspectorAtmosphere = renderer.getAtmosphereSettings();
        return;
    }

    const auto& nodes = renderer.getSceneNodes();
    if (g_selectedNode < 0 || g_selectedNode >= static_cast<int>(nodes.size())) {
        g_nameBuffer[0] = '\0';
        return;
    }

    const auto& node = nodes[g_selectedNode];
    std::strncpy(g_nameBuffer, node.name.c_str(), sizeof(g_nameBuffer) - 1);
    g_nameBuffer[sizeof(g_nameBuffer) - 1] = '\0';
    g_inspectorPosition = node.position;
    g_inspectorRotation = node.rotation;
    g_inspectorScale = node.scale;
    g_inspectorColor = node.color;
    g_lastSelectedNode = g_selectedNode;
    g_lastSelectedAtmosphere = false;
}

static void drawInspector(Renderer& renderer) {
    ImGui::SetNextWindowSize(ImVec2(360, 320), ImGuiCond_Once);
    ImGui::Begin("Inspector");

    const auto& nodes = renderer.getSceneNodes();
    if (g_selectedAtmosphere) {
        if (!g_lastSelectedAtmosphere) {
            syncInspectorState(renderer);
        }

        ImGui::Text("Type: Atmosphere");
        ImGui::Separator();
        ImGui::Checkbox("Enable Atmosphere", &g_inspectorAtmosphere.enabled);
        ImGui::ColorEdit3("Sky Color", &g_inspectorAtmosphere.skyColor[0]);
        ImGui::ColorEdit3("Fog Color", &g_inspectorAtmosphere.fogColor[0]);
        ImGui::DragFloat("Fog Density", &g_inspectorAtmosphere.fogDensity, 0.005f, 0.0f, 1.0f, "%.3f");
        if (ImGui::Button("Apply Atmosphere Settings")) {
            renderer.setAtmosphereSettings(g_inspectorAtmosphere);
        }
    } else if (g_selectedNode >= 0 && g_selectedNode < static_cast<int>(nodes.size())) {
        if (g_lastSelectedNode != g_selectedNode) {
            syncInspectorState(renderer);
        }

        const auto& node = renderer.getSceneNode(g_selectedNode);
        ImGui::InputText("Name", g_nameBuffer, sizeof(g_nameBuffer));
        renderer.setSceneNodeName(g_selectedNode, g_nameBuffer);

        ImGui::Text("Type: %s", node.type == Renderer::SceneNodeType::Cube ? "Cube" : "Unknown");
        ImGui::Separator();

        if (node.type == Renderer::SceneNodeType::Cube) {
            if (ImGui::DragFloat3("Position", &g_inspectorPosition[0], 0.1f, -10.0f, 10.0f)) {
                renderer.setSceneNodeTransform(g_selectedNode, g_inspectorPosition, g_inspectorRotation, g_inspectorScale);
            }

            if (ImGui::DragFloat3("Rotation", &g_inspectorRotation[0], 1.0f, -180.0f, 180.0f)) {
                renderer.setSceneNodeTransform(g_selectedNode, g_inspectorPosition, g_inspectorRotation, g_inspectorScale);
            }

            if (ImGui::DragFloat3("Scale", &g_inspectorScale[0], 0.05f, 0.01f, 10.0f)) {
                renderer.setSceneNodeTransform(g_selectedNode, g_inspectorPosition, g_inspectorRotation, g_inspectorScale);
            }

            if (ImGui::ColorEdit3("Color", &g_inspectorColor[0])) {
                renderer.setSceneNodeColor(g_selectedNode, g_inspectorColor);
            }
        }
    } else {
        ImGui::Text("No selection.");
    }

    ImGui::Separator();
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    ImGui::End();
}

static void drawSceneHierarchyWindow(Renderer& renderer) {
    ImGui::SetNextWindowSize(ImVec2(280, 320), ImGuiCond_Once);
    ImGui::Begin("Scene Hierarchy");
    ImGui::Text("Scene Graph");
    ImGui::Separator();

    const auto& nodes = renderer.getSceneNodes();
    if (nodes.empty()) {
        ImGui::Text("No nodes in scene.");
    }

    bool atmosphereSelected = g_selectedAtmosphere;
    if (ImGui::Selectable("Atmosphere", atmosphereSelected)) {
        g_selectedAtmosphere = true;
        g_selectedNode = -1;
        g_lastSelectedNode = -1;
        g_lastSelectedAtmosphere = false;
    }

    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        bool selected = (g_selectedNode == i) && !g_selectedAtmosphere;
        if (ImGui::Selectable(nodes[i].name.c_str(), selected)) {
            g_selectedAtmosphere = false;
            g_selectedNode = i;
            g_lastSelectedNode = -1;
            g_lastSelectedAtmosphere = false;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Cube")) {
        int newIndex = static_cast<int>(nodes.size());
        renderer.addCubeNode("Cube " + std::to_string(nodes.size() + 1));
        g_selectedNode = newIndex;
        g_lastSelectedNode = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Node") && g_selectedNode >= 0) {
        renderer.removeSceneNode(g_selectedNode);
        g_selectedNode = -1;
        g_lastSelectedNode = -1;
    }

    ImGui::End();
}

void drawEditorPanels(Renderer& renderer) {
    drawInspector(renderer);
    drawSceneHierarchyWindow(renderer);
}

} // namespace editor
