
#pragma once

#include "wisp.hpp"
#include "window.hpp"
#include "scene_graph/scene_graph.hpp"
#include "resources.hpp"
#include "imgui/imgui.hpp"
#include "debug_camera.hpp"
#include "physics_engine.hpp"
#include "phys_node.hpp"

namespace viknell_scene
{

	static std::shared_ptr<DebugCamera> camera;
	static std::shared_ptr<wr::LightNode> directional_light_node;
	static std::shared_ptr<wr::MeshNode> test_model;
	static float t = 0;

	void CreateScene(wr::SceneGraph* scene_graph, wr::Window* window, phys::PhysicsEngine& phys_engine)
	{
		camera = scene_graph->CreateChild<DebugCamera>(nullptr, 90.f, (float)window->GetWidth() / (float)window->GetHeight());
		camera->SetPosition({ 0, 0, -1 });
		camera->SetSpeed(10);

		scene_graph->m_skybox = resources::equirectangular_environment_map;
		auto skybox = scene_graph->CreateChild<wr::SkyboxNode>(nullptr, resources::equirectangular_environment_map);

		{
			// Geometry
			auto roof = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::plane_model);
			auto roof_light = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::light_model);
			auto back_wall = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::plane_model);
			auto left_wall = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::plane_model);
			auto right_wall = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::plane_model);
			test_model = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::test_model);
			auto sphere = scene_graph->CreateChild<wr::MeshNode>(nullptr, resources::sphere_model);
			sphere->SetPosition({ -1, 1, 1 });
			sphere->SetScale({ 0.6f, 0.6f, 0.6f });
			roof->SetPosition({ 0, -1, 0 });
			roof->SetRotation({ 90_deg, 0, 0 });
			roof_light->SetPosition({ 0, -0.999, 0 });
			roof_light->SetRotation({ 90_deg, 0, 0 });
			roof_light->SetScale({ 0.7, 0.7, 0.7 });
			back_wall->SetPosition({ 0, 0, 1 });
			left_wall->SetPosition({ -1, 0, 0 });
			left_wall->SetRotation({ 0, -90_deg, 0 });
			right_wall->SetPosition({ 1, 0, 0 });
			right_wall->SetRotation({ 0, 90_deg, 0 });
			test_model->SetPosition({ 0, 1, 0 });
			test_model->SetRotation({ 0,0,180_deg });
			test_model->SetScale({ 0.01f,0.01f,0.01f });

			// Lights
			directional_light_node = scene_graph->CreateChild<wr::LightNode>(nullptr, wr::LightType::DIRECTIONAL, DirectX::XMVECTOR{ 5, 5, 5 });
			//point_light_0->SetRadius(3.0f);
			directional_light_node->SetRotation({ 20.950, 0.98, 0 });
			directional_light_node->SetPosition({ 0, -6.527, 17.9 });

			auto point_light_1 = scene_graph->CreateChild<wr::LightNode>(nullptr, wr::LightType::POINT, DirectX::XMVECTOR{ 5, 0, 0 });
			point_light_1->SetRadius(2.0f);
			point_light_1->SetPosition({ 0.5, 0, -0.3 });

			auto point_light_2 = scene_graph->CreateChild<wr::LightNode>(nullptr, wr::LightType::POINT, DirectX::XMVECTOR{ 0, 0, 5 });
			point_light_2->SetRadius(3.0f);
			point_light_2->SetPosition({ -0.7, 0.5, -0.3 });

			//auto dir_light = scene_graph->CreateChild<wr::LightNode>(nullptr, wr::LightType::DIRECTIONAL, DirectX::XMVECTOR{ 1, 1, 1 });
		}

		auto floor = scene_graph->CreateChild<PhysicsMeshNode>(nullptr, resources::plane_model);
		//floor->SetupSimpleBoxColl(phys_engine, { 1, 0.01, 1 });
		floor->SetupConvex(phys_engine, resources::plane_model);
		floor->SetPosition({ 0, 1, 0 });
		floor->SetRotation({ -90_deg, 0, 0 });

		auto phys_sphere = scene_graph->CreateChild<PhysicsMeshNode>(nullptr, resources::sphere_model);
		phys_sphere->SetMass(1.f);
		phys_sphere->SetupSimpleSphereColl(phys_engine, 0.1f);
		phys_sphere->SetPosition({ 0, 0, 0 });
		phys_sphere->SetScale({ 0.1f, 0.1f, 0.1f });
		phys_sphere->SetRestitution(0.7f);
		floor->SetRestitution(1.f);
	}

	void UpdateScene()
	{
		t += 10.f * ImGui::GetIO().DeltaTime;

		auto pos = directional_light_node->m_position;
		pos.m128_f32[0] = (20.950) + (sin(t * 0.1) * 4);
		pos.m128_f32[1] = (-6.58) + (cos(t * 0.1) * 2);
		directional_light_node->SetPosition(pos);

		camera->Update(ImGui::GetIO().DeltaTime);
	}
} /* cube_scene */