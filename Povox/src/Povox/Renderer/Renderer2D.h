#pragma once
#include "Povox/Renderer/OrthographicCamera.h"
#include "Povox/Renderer/Camera.h"
#include "Povox/Renderer/EditorCamera.h"

#include "Povox/Scene/Components.h"

#include "Povox/Renderer/Buffer.h"
#include "Povox/Renderer/Framebuffer.h"
#include "Povox/Renderer/Pipeline.h"
#include "Povox/Renderer/Renderable.h"
#include "Povox/Renderer/Renderer.h"
#include "Povox/Renderer/RenderPass.h"
#include "Povox/Renderer/Texture.h"
#include "Povox/Renderer/SubTexture2D.h"

#include "Povox/Systems/MaterialSystem.h"

#include <glm/glm.hpp>

namespace Povox {

	struct Renderer2DSpecification
	{
		static const uint32_t MaxQuads = 60000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; //TODO: needs to be dynamic to the GPU, comes from Renderer Capabilities

		//TODO: Temp, move to scene
		uint32_t ViewportWidth = 0;
		uint32_t ViewportHeight = 0;
	};

	// Stats
	struct Renderer2DStatistics
	{
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;


		uint32_t GetTotalVertexCount() { return QuadCount * 4; }
		uint32_t GetTotalIndexCount() { return QuadCount * 6; }
	};

	class Renderer2D
	{
	public:
		Renderer2D(const Renderer2DSpecification& specs);
		~Renderer2D() = default;

		// Core
		bool Init();
		void Shutdown();

		void OnResize(uint32_t width, uint32_t height);

		
		// Scene
		void BeginScene(const Camera& camera, const glm::mat4& transform);
		void BeginScene(const OrthographicCamera& camera);
		void BeginScene(const EditorCamera& camera);
		void EndScene();

		void Flush();

	// Primitives
		//Flat
		void DrawQuad(const glm::vec2& position, const glm::vec2 size, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2 size, float rotation, const glm::vec4& color);
		void DrawQuad(const glm::vec3& position, const glm::vec2 size, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2 size, float rotation, const glm::vec4& color);
		void DrawQuad(const glm::mat4& transform, const glm::vec4& color, UUID entityID = UUID(0));

		//Textured
		void DrawQuad(const glm::vec2& position, const glm::vec2 size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2 size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawQuad(const glm::vec3& position, const glm::vec2 size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2 size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4 & tintingColor = glm::vec4(1.0f));
		void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f), UUID entityID = UUID(0));
		
		//Textures from SubTextures
		void DrawQuad(const glm::vec2& position, const glm::vec2 size, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2 size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawQuad(const glm::vec3& position, const glm::vec2 size, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2 size, float rotation, const Ref<SubTexture2D>& subTexture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f));
		void DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintingColor = glm::vec4(1.0f), UUID entityID = UUID(0));


		void DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, UUID enittyID);
		void DrawFullscreenQuad();

		const Renderer2DStatistics& GetStatistics() const;
		void ResetStatistics();

		inline const Ref<Image2D> GetFinalImage() const { return m_FinalImage; }

	private:
		void NextBatch();
		void StartBatch();

	private:
		Renderer2DSpecification m_Specification{};

		//Final
		Ref<Image2D> m_FinalImage = nullptr;

		// Common
		Ref<Texture2D> m_WhiteTexture;
		uint32_t m_WhiteTextureSlot;
		Ref<Texture2D> m_GreenTexture = nullptr;

		CameraUniform m_CameraUniform{};
		Ref<UniformBuffer> m_CameraData = nullptr;

		SceneUniform m_SceneUniform{};
		Ref<UniformBuffer> m_SceneData = nullptr;

		ObjectUniform m_ObjectUnifrom{};
		Ref<StorageBuffer> m_ObjectData = nullptr;

		// 2D Renderpasses
		// Quads
		Povox::ShaderHandle m_QuadShaderHandle;
		Ref<RenderPass> m_QuadRenderpass = nullptr;
		Ref<Framebuffer> m_QuadFramebuffer = nullptr;
		Ref<Pipeline> m_QuadPipeline = nullptr;
		Ref<Material> m_QuadMaterial = nullptr;

		std::vector<Ref<Buffer>> m_QuadVertexBuffers;
		std::vector<QuadVertex*> m_QuadVertexBufferBases;
		QuadVertex* m_QuadVertexBufferPtr = nullptr;

		std::vector<Ref<Buffer>> m_QuadIndexBuffers;
		uint32_t m_QuadIndexCount = 0;

		glm::vec4 m_QuadVertexPositions[4];

		// FullscreenQuad
		Povox::ShaderHandle m_FullscreenQuadShaderHandle;
		Ref<RenderPass> m_FullscreenQuadRenderpass = nullptr;
		Ref<Pipeline> m_FullscreenQuadPipeline = nullptr;

		Ref<Buffer> m_FullscreenQuadVertexBuffer = nullptr;

		Ref<Buffer> m_FullscreenQuadIndexBuffer = nullptr;
		Ref<Material> m_FullscreenQuadMaterial = nullptr;

		// TODO: Lines
		// TODO: Font
		// TODO: Polygons


		//TODO: temp object list
		//std::vector<Renderable> m_RenderedObjects;


		// Stats
		Renderer2DStatistics m_Stats{};
	};

}
