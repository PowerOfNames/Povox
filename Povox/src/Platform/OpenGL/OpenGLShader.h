#pragma once

#include "Povox/Renderer/Shader.h"

#include <glm/glm.hpp>

namespace Povox
{
	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader();

		virtual void Bind() const override;
		virtual void Unbind() const override;
		
		virtual void UploadUniformInt(const std::string& name, int value);
		
		virtual void UploadUniformFloat(const std::string& name, float value);
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& vector);
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& vector);
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& vector);

		virtual void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);

	private:
		uint32_t m_RendererID;
	};
}
