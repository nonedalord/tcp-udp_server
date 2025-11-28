#include "Application.h"

volatile std::atomic<bool> Application::g_terminated = false;

Application::Application() : m_logger(boost::log::keywords::channel = "Application") {}

void Application::SignalHandler(int s) 
{
    g_terminated.store(true);
}


int Application::Run(std::string_view port)
{
    try
    {
        int new_port = GetIntPort(port);
        LogHelper::InitLogging();
        LogHelper::SetLevel(boost::log::trivial::trace);
        SetupSignalHandlers();
        InitServer(new_port);
        LOG(m_logger, LogHelper::info, "Server started");
    }
    catch (const std::exception& err)
    {
        LOG(m_logger, LogHelper::error, "Error while init TCP/UDP server: " << err.what());
        LOG(m_logger, LogHelper::info, "Server stopped");
        return EXIT_FAILURE;
    }
    MainLoop();
    LOG(m_logger, LogHelper::info, "Server stopped");
    return EXIT_SUCCESS;
}

void Application::InitServer(int port)
{
    m_server = std::make_unique<TCPUPDServer>();
    m_server->SetShutdownCallback([]
    {
        g_terminated.store(true);
    }
    );
    m_server->Init(port);
    unsigned int max_threads = 8;
    m_server->ListenAsync(8);
}

int Application::GetIntPort(std::string_view port)
{
    try
    {
        return std::atoi(port.data());
    }
    catch(const std::exception& err)
    {
        throw std::runtime_error("converting port to int error: " + static_cast<std::string>(err.what()));
    }
}

void Application::SetupSignalHandlers()
{
    struct sigaction sa;
    sa.sa_handler = &SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, nullptr) == -1 || sigaction(SIGTERM, &sa, nullptr) == -1)
    {
        throw std::runtime_error("failed to set signal handlers");
    }
}

void Application::MainLoop()
{
    while (!g_terminated.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
