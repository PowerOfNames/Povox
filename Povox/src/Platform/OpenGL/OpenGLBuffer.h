#pragma once

#include "Povox/Renderer/Buffer.h"

namespace Povox {

//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Vertex buffer //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer(uint32_t size);
		OpenGLVertexBuffer(float* vertices, uint32_t size);
		virtual ~OpenGLVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void Submit(const void* data, uint32_t size) const override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout;  }

		inline uint32_t GetID() const override { return m_RendererID; }

	private:
		uint32_t m_RendererID;
		BufferLayout m_Layout;
	};

//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Index buffer ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
		OpenGLIndexBuffer(uint32_t count);
		virtual ~OpenGLIndexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const	override;

		virtual void Submit(const void* data, uint32_t size) const override;

		inline virtual uint32_t GetCount() const override { return m_Count; }

	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	};
}
