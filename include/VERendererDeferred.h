#pragma once

namespace vve {
	class RendererDeferred : public System {

	public:
		RendererDeferred(const std::string& systemName, Engine& engine, const std::string& windowName);
		virtual ~RendererDeferred();

	private:
		bool OnInit(const Message& message);

		std::string m_windowName;
	};

}	// namespace vve
