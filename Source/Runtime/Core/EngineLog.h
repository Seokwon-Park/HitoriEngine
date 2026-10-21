#pragma once

enum class ELogVerbosity
{
	Input,
	Info,
	Warning,
	Error
};

class ILogSink
{
public:
	virtual void OnLog(ELogVerbosity, const FString&) = 0;
};

class FLog
{
public:
    static void AddSink(ILogSink* Sink);
    static void RemoveSink(ILogSink* Sink);
    static void Emit(ELogVerbosity Verbosity, const FString& Message);

    template <typename... Args>
    static void Log(ELogVerbosity Verbosity, std::format_string<Args...> Fmt, Args&&... InArgs)
    {
        Emit(Verbosity, std::format(Fmt, std::forward<Args>(InArgs)...));
    }
};

#define HTR_LOG(Verbosity, ...) FLog::Log(ELogVerbosity::Verbosity, __VA_ARGS__)