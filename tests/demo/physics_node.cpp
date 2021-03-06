/*!
 * Copyright 2019 Breda University of Applied Sciences and Team Wisp (Viktor Zoutman, Emilio Laiso, Jens Hagen, Meine Zeinstra, Tahar Meijs, Koen Buitenhuis, Niels Brunekreef, Darius Bouma, Florian Schut)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "physics_node.hpp"
#include "physics_engine.hpp"

PhysicsMeshNode::~PhysicsMeshNode()
{
	if(m_shape)
	{
		delete m_shape;
	}
	
	if(m_rigid_body)
	{
		delete m_rigid_body->getMotionState();
		m_phys_engine.phys_world->removeRigidBody(m_rigid_body);
		delete m_rigid_body;
	}
	
	if(m_shapes.has_value())
	{
		for(auto* shape : m_shapes.value())
		{
			delete shape;
		}
	}

	if(m_rigid_bodies.has_value())
	{
		for(auto* rigid_body : m_rigid_bodies.value())
		{
			delete rigid_body->getMotionState();
			m_phys_engine.phys_world->removeRigidBody(rigid_body);
			delete rigid_body;
		}
	}
}

void PhysicsMeshNode::SetMass(float mass)
{
	m_mass = mass;

	if (m_rigid_body)
	{
		btVector3 local_intertia(0, 0, 0);
		m_shape->calculateLocalInertia(mass, local_intertia);
		m_rigid_body->setMassProps(mass, local_intertia);
	}
}

void PhysicsMeshNode::SetRestitution(float value)
{
	if (m_rigid_bodies.has_value())
	{
		for (auto& body : m_rigid_bodies.value())
		{
			body->setRestitution(value);
		}
	}
	else
	{
		m_rigid_body->setRestitution(value);
	}
}

void PhysicsMeshNode::SetupSimpleBoxColl(phys::PhysicsEngine& phys_engine, DirectX::XMVECTOR scale)
{
	btTransform transform;
	transform.setIdentity();

	m_shape = phys_engine.CreateBoxShape(phys::util::DXV3toBV3(scale));
	m_rigid_body = phys_engine.CreateRigidBody(btScalar(m_mass), transform, m_shape);
}

void PhysicsMeshNode::SetupSimpleSphereColl(phys::PhysicsEngine& phys_engine, float radius)
{
	btTransform transform;
	transform.setIdentity();

	m_shape = phys_engine.CreateSphereShape(radius);
	m_rigid_body = phys_engine.CreateRigidBody(btScalar(m_mass), transform, m_shape);
}

void PhysicsMeshNode::SetupConvex(phys::PhysicsEngine& phys_engine, wr::ModelData* model)
{
	m_rigid_bodies = std::vector<btRigidBody*>();
	m_shapes = std::vector<btCollisionShape*>();

	auto hulls = phys_engine.CreateConvexShape(model);

	for (auto& hull : hulls)
	{
		m_shapes->push_back(hull);

		btTransform transform;
		transform.setIdentity();
		auto body = phys_engine.CreateRigidBody(0.f, transform, hull);
		m_rigid_bodies->push_back(body);
	}
}

void PhysicsMeshNode::SetupTriangleMesh(phys::PhysicsEngine& phys_engine, wr::ModelData* model)
{
	m_rigid_bodies = std::vector<btRigidBody*>();
	m_shapes = std::vector<btCollisionShape*>();

	auto hulls = phys_engine.CreateTriangleMeshShape(model);

	for (auto& hull : hulls)
	{
		m_shapes->push_back(hull);

		btTransform transform;
		transform.setIdentity();
		auto body = phys_engine.CreateRigidBody(0.f, transform, hull);
		m_rigid_bodies->push_back(body);
	}
}

void PhysicsMeshNode::SetPosition(DirectX::XMVECTOR position)
{
	if (m_rigid_bodies.has_value())
	{
		for (auto& body : m_rigid_bodies.value())
		{
			auto& world_trans = body->getWorldTransform();
			world_trans.setOrigin(phys::util::DXV3toBV3(position));
		}
	}
	else if (m_rigid_body)
	{
		auto& world_trans = m_rigid_body->getWorldTransform();
		world_trans.setOrigin(phys::util::DXV3toBV3(position));
	}

	m_position = position;
	SignalTransformChange();
}

void PhysicsMeshNode::SetRotation(DirectX::XMVECTOR roll_pitch_yaw)
{
	auto quat = DirectX::XMQuaternionRotationRollPitchYawFromVector(roll_pitch_yaw);
	if (m_rigid_bodies.has_value())
	{
		for (auto& body : m_rigid_bodies.value())
		{
			auto& world_trans = body->getWorldTransform();
			world_trans.setRotation(btQuaternion(quat.m128_f32[0], quat.m128_f32[1], quat.m128_f32[2], quat.m128_f32[3]));
		}
	}
	else if (m_rigid_body)
	{
		auto& world_trans = m_rigid_body->getWorldTransform();
		world_trans.setRotation(btQuaternion(quat.m128_f32[0], quat.m128_f32[1], quat.m128_f32[2], quat.m128_f32[3]));
	}

	m_rotation_radians = roll_pitch_yaw;
	SignalTransformChange();
}

void PhysicsMeshNode::SetScale(DirectX::XMVECTOR scale)
{
	if (m_rigid_bodies.has_value())
	{
		for (auto& shape : m_shapes.value())
		{
			shape->setLocalScaling(phys::util::DXV3toBV3(scale));
		}
	}
	else if (m_shape)
	{
		m_shape->setLocalScaling(phys::util::DXV3toBV3(scale));
	}

	m_scale = scale;
	SignalTransformChange();
}