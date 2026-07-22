/// editor/EditorPanels.cpp

#include "EditorPanels.h"
#include "imgui.h"
#include <string>
#include <cstring>
#include <cstdio>

namespace editor {

// ============================================================
// Editor state — persists across frames
// ============================================================

struct EditorState {
    // selection
    int  selectedNode       = -1;
    bool atmosphereSelected = false;
    GizmoMode gizmoMode      = GizmoMode::Translate;

    // inspector local copies — only written to renderer on change
    char      nameBuffer[128]  = {};
    glm::vec3 position         = glm::vec3(0.0f);
    glm::vec3 rotation         = glm::vec3(0.0f);
    glm::vec3 scale            = glm::vec3(1.0f);
    glm::vec3 color            = glm::vec3(1.0f);
    Renderer::AtmosphereSettings atmosphere;

    // track what was last loaded into the inspector so we only
    // refresh local copies when selection actually changes
    int  loadedNode            = -2; // -2 = nothing loaded yet
    bool loadedAtmosphere      = false;
};

static EditorState s;

static void loadNode(const Renderer& renderer, int index);

int getSelectedNode() { return s.selectedNode; }

void setSelectedNode(int node) { s.selectedNode = node; s.atmosphereSelected = false; s.loadedNode = -2; }

GizmoMode getGizmoMode() { return s.gizmoMode; }

void setGizmoMode(GizmoMode mode) { s.gizmoMode = mode; }

void refreshSelectedNodeState(Renderer& renderer) {
    if (s.selectedNode < 0) return;
    const auto& nodes = renderer.getSceneNodes();
    if (s.selectedNode >= 0 && s.selectedNode < static_cast<int>(nodes.size()))
        loadNode(renderer, s.selectedNode);
}

// ============================================================
// Load node data into local inspector buffers
// Called only when selection changes, not every frame
// ============================================================

static void loadNode(const Renderer& renderer, int index) {
    const auto& node = renderer.getSceneNodes()[index];
    std::strncpy(s.nameBuffer, node.name.c_str(), sizeof(s.nameBuffer) - 1);
    s.nameBuffer[sizeof(s.nameBuffer) - 1] = '\0';
    s.position  = node.position;
    s.rotation  = node.rotation;
    s.scale     = node.scale;
    s.color     = node.color;
    s.loadedNode = index;
    s.loadedAtmosphere = false;
}

static void loadAtmosphere(const Renderer& renderer) {
    s.atmosphere = renderer.getAtmosphereSettings();
    s.loadedAtmosphere = true;
    s.loadedNode = -2;
}

// ============================================================
// Scene Hierarchy panel
// ============================================================

static void drawHierarchy(Renderer& renderer) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(240, 400), ImGuiCond_Once);
    ImGui::Begin("Scene Hierarchy");

    // Atmosphere entry
    if (ImGui::Selectable("  Atmosphere", s.atmosphereSelected)) {
        s.atmosphereSelected = true;
        s.selectedNode       = -1;
        s.loadedNode         = -2; // force reload next frame
    }

    ImGui::Separator();
    ImGui::Text("Objects");
    ImGui::Separator();

    const auto& nodes = renderer.getSceneNodes();
    for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
        bool selected = (s.selectedNode == i) && !s.atmosphereSelected;

        // label with index prefix so duplicate names are distinguishable
        char label[160];
        std::snprintf(label, sizeof(label), "  [%d] %s", i, nodes[i].name.c_str());

        if (ImGui::Selectable(label, selected)) {
            s.selectedNode       = i;
            s.atmosphereSelected = false;
            s.loadedNode         = -2; // force reload
        }
    }

    ImGui::Separator();

    if (ImGui::Button("+ Add Cube")) {
        int newIndex = static_cast<int>(nodes.size());
        renderer.addCubeNode("Cube " + std::to_string(newIndex + 1));
        s.selectedNode       = newIndex;
        s.atmosphereSelected = false;
        s.loadedNode         = -2;
    }

    ImGui::SameLine();

    // only show remove if something is selected
    bool canRemove = s.selectedNode >= 0 &&
                     s.selectedNode < static_cast<int>(renderer.getSceneNodes().size());
    if (!canRemove) ImGui::BeginDisabled();
    if (ImGui::Button("- Remove")) {
        renderer.removeSceneNode(s.selectedNode);
        s.selectedNode = -1;
        s.loadedNode   = -2;
    }
    if (!canRemove) ImGui::EndDisabled();

    ImGui::End();
}

// ============================================================
// Inspector panel
// ============================================================

static void drawInspector(Renderer& renderer) {
    ImGui::SetNextWindowPos(ImVec2(10, 420), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 340), ImGuiCond_Once);
    ImGui::Begin("Inspector");

    // ---- Atmosphere inspector ----
    if (s.atmosphereSelected) {
        if (!s.loadedAtmosphere)
            loadAtmosphere(renderer);

        ImGui::Text("Atmosphere");
        ImGui::Separator();

        ImGui::Checkbox("Enabled", &s.atmosphere.enabled);

        ImGui::ColorEdit3("Sky Color", &s.atmosphere.skyColor[0]);
        ImGui::ColorEdit3("Fog Color", &s.atmosphere.fogColor[0]);
        ImGui::SliderFloat("Fog Density", &s.atmosphere.fogDensity, 0.0f, 1.0f, "%.4f");

        ImGui::Spacing();
        if (ImGui::Button("Apply", ImVec2(120, 0)))
            renderer.setAtmosphereSettings(s.atmosphere);
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(120, 0)))
            loadAtmosphere(renderer); // reload from renderer

        ImGui::End();
        return;
    }

    // ---- Node inspector ----
    const auto& nodes = renderer.getSceneNodes();
    bool validNode = s.selectedNode >= 0 &&
                     s.selectedNode < static_cast<int>(nodes.size());

    if (!validNode) {
        ImGui::TextDisabled("No selection.");
        ImGui::End();
        return;
    }

    // refresh local state only when selection changed
    if (s.loadedNode != s.selectedNode)
        loadNode(renderer, s.selectedNode);

    ImGui::Text("Node [%d]", s.selectedNode);
    ImGui::Separator();

    // name — only push to renderer when editing is finished (Enter/unfocus)
    if (ImGui::InputText("Name", s.nameBuffer, sizeof(s.nameBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        renderer.setSceneNodeName(s.selectedNode, s.nameBuffer);
    }
    // also commit on deactivation (click away)
    if (ImGui::IsItemDeactivatedAfterEdit())
        renderer.setSceneNodeName(s.selectedNode, s.nameBuffer);

    ImGui::Spacing();
    ImGui::Text("Gizmo");
    ImGui::Separator();
    if (ImGui::RadioButton("Translate", s.gizmoMode == GizmoMode::Translate))
        s.gizmoMode = GizmoMode::Translate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", s.gizmoMode == GizmoMode::Rotate))
        s.gizmoMode = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", s.gizmoMode == GizmoMode::Scale))
        s.gizmoMode = GizmoMode::Scale;

    ImGui::Spacing();
    ImGui::Text("Transform");
    ImGui::Separator();

    bool transformChanged = false;
    transformChanged |= ImGui::DragFloat3("Position", &s.position[0], 0.05f);
    transformChanged |= ImGui::DragFloat3("Rotation", &s.rotation[0], 0.5f, -180.0f, 180.0f);
    transformChanged |= ImGui::DragFloat3("Scale",    &s.scale[0],    0.02f, 0.001f, 20.0f);

    // push transform only when the drag is released or value changed
    // IsItemDeactivatedAfterEdit fires when the user releases the drag
    if (transformChanged)
        renderer.setSceneNodeTransform(s.selectedNode, s.position, s.rotation, s.scale);

    ImGui::Spacing();
    ImGui::Text("Appearance");
    ImGui::Separator();

    if (ImGui::ColorEdit3("Color", &s.color[0]))
        renderer.setSceneNodeColor(s.selectedNode, s.color);

    ImGui::Spacing();
    ImGui::Separator();

    // reset button — reloads from renderer
    if (ImGui::Button("Reset to saved", ImVec2(160, 0)))
        loadNode(renderer, s.selectedNode);

    ImGui::End();
}

// ============================================================
// Stats overlay — top right, non-interactive
// ============================================================

static void drawStatsOverlay() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 200.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(190, 70), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs     |
        ImGuiWindowFlags_NoNav        |
        ImGuiWindowFlags_NoMove;
    ImGui::Begin("##stats", nullptr, flags);
    ImGui::Text("%.1f FPS  (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::Text("Display: %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::End();
}

// ============================================================
// Public entry point
// ============================================================

void drawEditorPanels(Renderer& renderer) {
    drawHierarchy(renderer);
    drawInspector(renderer);
    drawStatsOverlay();
}

} // namespace editor
