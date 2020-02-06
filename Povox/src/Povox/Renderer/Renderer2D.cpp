#include "pxpch.h"
#include "Povox/Renderer/Renderer2D.h"

#include "Povox/Renderer/VertexArray.h"
#include "Povox/Renderer/Shader.h"
#include "Povox/Renderer/RenderCommand.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Povox {

	struct Render2DStorage
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> TextureShader;
		Ref<Texture> WhiteTexture;
	};

	static Render2DStorage* s_QuadData;

	void Renderer2D::Init()
	{
		s_QuadData = new Render2DStorage();
		s_QuadData->QuadVertexArray = VertexArray::Create();

		float squareVertices[4 * 5] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};
		Ref<VertexBuffer> squareVB = VertexBuffer::Create(squareVertices, sizeof(squareVertices));
		squareVB->SetLayout({
			{ Povox::ShaderDataType::Float3, "a_Position" },
			{ Povox::ShaderDataType::Float2, "a_TexCoord" }
			});
		s_QuadData->QuadVertexArray->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		Ref<IndexBuffer> squareIB = IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));
		s_QuadData->QuadVertexArray->SetIndexBuffer(squareIB);

		s_QuadData->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_QuadData->WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		s_QuadData->TextureShader = Shader::Create("assets/shaders/Texture.glsl");

		s_QuadData->TextureShader->Bind();
		s_QuadData->TextureShader->SetInt("u_Texture", 0);
	}

	void Renderer2D::Shutdown()
	{
		delete s_QuadData;
	}

	void Renderer2D::BeginScene(OrthographicCamera& camera)
	{
		s_QuadData->TextureShader->Bind();
		s_QuadData->TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
	}

	void Renderer2D::EndScene()
	{
	}

	/*Renders a quad in 2D without depth*/
	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2 size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	/*Renders a quad with possible depth*/
	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2 size, const glm::vec4& color)
	{
		s_QuadData->TextureShader->SetFloat4("u_Color", color);
		s_QuadData->WhiteTexture->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));
		s_QuadData->TextureShader->SetMat4("u_Transform", transform);

		s_QuadData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_QuadData->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2 size, const Ref<Texture2D>& texture)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2 size, const Ref<Texture2D>& texture)
	{
		s_QuadData->TextureShader->SetFloat4("u_Color", glm::vec4(1.0f));
		texture->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));
		s_QuadData->TextureShader->SetMat4("u_Transform", transform);
		
		s_QuadData->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_QuadData->QuadVertexArray);
	}

}