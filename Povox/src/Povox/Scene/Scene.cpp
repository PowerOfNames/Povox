#include "pxpch.h"
#include "Povox/Scene/Scene.h"

#include "Povox/Scene/Components.h"
#include "Povox/Renderer/Renderer2D.h"
#include <glm/glm.hpp>

#include "Povox/Scene/Entity.h"

namespace Povox {

	Scene::Scene()
	{

	}

	Scene::~Scene()
	{

	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>(name);
		tag.Tag = name.empty() ? "Unnamed Entity" : name;

		return entity;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		//Render sprites
		Camera* mainCamera = nullptr;
		glm::mat4* cameraTransform = nullptr;
		{
			auto group = m_Registry.group<CameraComponent, TransformComponent>();
			for (auto entity : group)
			{
				auto& [camera, transform] = group.get<CameraComponent, TransformComponent>(entity);

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = &transform.Transform;
					break;
				}
			}
		}

		if(mainCamera)
		{
			auto group = m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
			Renderer2D::BeginScene(mainCamera->GetProjection(), *cameraTransform);
			for (auto entity : group)
			{
				auto& [sprite, transform] = group.get<SpriteRendererComponent, TransformComponent>(entity);

				Renderer2D::DrawQuad(transform, sprite.Color);
			}
			Renderer2D::EndScene();
		}
	}
}