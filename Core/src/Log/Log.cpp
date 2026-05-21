#include "Log.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

namespace CHEngine {

	struct LoggerImpl
	{
		Ref<spdlog::logger> CoreLogger;
		Ref<spdlog::logger> ModuleLogger;
		Ref<spdlog::logger> ClientLogger;
	};

	static LoggerImpl* s_Instance = nullptr;

	Logger::Logger()
	{
		m_Impl = MakeScope<LoggerImpl>();

		spdlog::set_pattern("%^[%T] %n: %v%$");

		m_Impl->CoreLogger = spdlog::stdout_color_mt("CHENGINE");
		m_Impl->CoreLogger->set_level(spdlog::level::trace);

		m_Impl->ModuleLogger = spdlog::stdout_color_mt("MODULE");
		m_Impl->ModuleLogger->set_level(spdlog::level::trace);

		m_Impl->ClientLogger = spdlog::stdout_color_mt("APP");
		m_Impl->ClientLogger->set_level(spdlog::level::trace);

		s_Instance = m_Impl.get();
	}

	Logger::~Logger()
	{
		s_Instance = nullptr;
	}

	void Logger::CoreLog(int level, std::string msg)
	{
		if (s_Instance)
			s_Instance->CoreLogger->log(static_cast<spdlog::level::level_enum>(level), msg);
	}

	void Logger::ModuleLog(int level, std::string msg)
	{
		if (s_Instance)
			s_Instance->ModuleLogger->log(static_cast<spdlog::level::level_enum>(level), msg);
	}

	void Logger::AppLog(int level, std::string msg)
	{
		if (s_Instance)
			s_Instance->ClientLogger->log(static_cast<spdlog::level::level_enum>(level), msg);
	}

} // namespace CHEngine
