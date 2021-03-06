#include "pxpch.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"
#include "Povox/Renderer/VoxelRenderer.h"

#include <glad/glad.h>

namespace Povox {

	static GLenum ShaderDataTypeToGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case Povox::ShaderDataType::Float:	return GL_FLOAT;
		case Povox::ShaderDataType::Float2:	return GL_FLOAT;
		case Povox::ShaderDataType::Float3:	return GL_FLOAT;
		case Povox::ShaderDataType::Float4:	return GL_FLOAT;
		case Povox::ShaderDataType::Mat3:	return GL_FLOAT;
		case Povox::ShaderDataType::Mat4:	return GL_FLOAT;
		case Povox::ShaderDataType::Int:	return GL_INT;
		case Povox::ShaderDataType::Int2:	return GL_INT;
		case Povox::ShaderDataType::Int3:	return GL_INT;
		case Povox::ShaderDataType::Int4:	return GL_INT;
		case Povox::ShaderDataType::Bool:	return GL_BOOL;
		}

		PX_CORE_ASSERT(false, "No such ShaderDataType defined!");
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
		PX_PROFILE_FUNCTION();


		glCreateVertexArrays(1, &m_RendererID);
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		PX_PROFILE_FUNCTION();


		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const
	{
		PX_PROFILE_FUNCTION();


		glBindVertexArray(m_RendererID);
	}

	void OpenGLVertexArray::Unbind() const
	{
		PX_PROFILE_FUNCTION();


		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		PX_PROFILE_FUNCTION();


		PX_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Bound vertex buffer has no set layout!");
		glBindVertexArray(m_RendererID);

		vertexBuffer->Bind();

		uint32_t index = 0;
		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout)
		{
			glEnableVertexArrayAttrib(vertexBuffer->GetID(), index);
			glVertexAttribPointer(index,
				element.GetComponentCount(),
				ShaderDataTypeToGLBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				(const void*)element.Offset);

			index++;
		}
		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SubmitVertexData(Vertex* vertices, size_t size) const
	{
		PX_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);
		for (Ref<VertexBuffer> buffer : m_VertexBuffers)
		{
			buffer->Submit(vertices, size);
		}
		glBindVertexArray(0);

	}

	void OpenGLVertexArray::SubmitIndices(uint32_t* indices, size_t size) const
	{
		PX_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);

		m_IndexBuffer->Submit(indices, size);

	}

	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		PX_PROFILE_FUNCTION();


		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}

}