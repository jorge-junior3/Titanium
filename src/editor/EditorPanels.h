/// editor/EditorPanels.h

#pragma once

#include "renderer/Renderer.h"
#include "Gizmo.h"

namespace editor {
int getSelectedNode(); // returns s.selectedNode
void setSelectedNode(int node);
GizmoMode getGizmoMode();
void setGizmoMode(GizmoMode mode);
void refreshSelectedNodeState(Renderer& renderer);
void drawEditorPanels(Renderer& renderer);

} // namespace editor
