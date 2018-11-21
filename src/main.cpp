#include <memory>

#include "wisp.hpp"
#include "render_tasks/d3d12_test_render_task.hpp"
#include "render_tasks/d3d12_imgui_render_task.hpp"

bool a;
bool b;
bool c;

void RenderEditor()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::DockSpaceOverViewport(true, nullptr, ImGuiDockNodeFlags_PassthruDockspace);

	auto& io = ImGui::GetIO();

	// Create dockable background
	ImGui::Begin("Hello World");
	ImGui::End();

	ImGui::Begin("Hello Me");
	ImGui::Text("Mouse Pos: (%f, %f)", io.MousePos.x, io.MousePos.y);
	ImGui::Checkbox("Checkbox 1", &b);
	ImGui::Checkbox("Checkbox 2", &c);
	ImGui::Button("Button 5");
	ImGui::End();

	ImGui::Begin("Hello You");
	ImGui::Button("Button 0");
	ImGui::Button("Button 1");
	ImGui::Button("Button 2");
	ImGui::Checkbox("Checkbox 0", &a);
	ImGui::Button("Button 3");
	ImGui::End();
}

void WispEntry()
{
	auto render_system = std::make_unique<wr::D3D12RenderSystem>();
	auto window = std::make_unique<wr::Window>(GetModuleHandleA(nullptr), "D3D12 Test App", 400, 400);

	render_system->Init(window.get());

	// Load custom model
	auto model_pool = render_system->CreateModelPool(1);
	wr::Model* model;
	{
		wr::MeshData<wr::Vertex> mesh;
		static const constexpr float size = 0.5f;
		mesh.m_indices = std::nullopt;
		mesh.m_vertices = {
			{ -size, -size, 0, 0, 0, 0, 1, 0 },
			{ size, -size, 0, 1, 0, 0, 1, 0 },
			{ -size, size, 0, 0, 1, 0, 1, 0 },
			{ size, size, 0, 1, 1, 0, 1, 0 },
		};

		model = model_pool->LoadCustom<wr::Vertex>({ mesh });
	}

	auto scene_graph = std::make_shared<wr::SceneGraph>(render_system.get());

	auto mesh_node = scene_graph->CreateChild<wr::MeshNode>(nullptr, model);

	auto mesh_node_2 = scene_graph->CreateChild<wr::MeshNode>(nullptr, model);
	mesh_node_2->SetPosition({ -2, -2, 5 });

	auto camera = scene_graph->CreateChild<wr::CameraNode>(nullptr, 1.74f, (float)window->GetWidth() / (float)window->GetHeight());
	camera->SetPosition(0, 0, -5);

	render_system->InitSceneGraph(*scene_graph.get());

	wr::FrameGraph frame_graph;
	//frame_graph.AddTask(wr::GetTestTask());
	frame_graph.AddTask(wr::GetImGuiTask(&RenderEditor));
	frame_graph.Setup(*render_system);

	while (window->IsRunning())
	{
		window->PollEvents();
		auto texture = render_system->Render(scene_graph, frame_graph);
	}
}

WISP_ENTRY(WispEntry)