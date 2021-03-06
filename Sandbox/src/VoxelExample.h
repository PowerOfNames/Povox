#pragma once

#include <Povox.h>

class VoxelExample : public Povox::Layer 
{
public:
	VoxelExample();
	~VoxelExample() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(float deltatime) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Povox::Event& e) override;

private:
	Povox::PerspectiveCameraController m_CameraController;

private:
	glm::vec3* m_CameraPosition;
	float m_DeltaTime = 0.0f;
	int m_Size = 3;
	bool m_DrawMode = false;
};
