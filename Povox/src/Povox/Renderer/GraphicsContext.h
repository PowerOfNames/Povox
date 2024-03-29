#pragma once

namespace Povox {

	class GraphicsContext
	{
	public:
		virtual ~GraphicsContext() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		static Ref<GraphicsContext> Create();
	};
}
