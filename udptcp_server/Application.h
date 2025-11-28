#include <csignal>
#include <stdexcept>
#include <string_view>
#include <memory>

#include "./server/Server.h"
#include "./logging/Logging.h"

class Application 
{
public:
    Application();
    int Run(std::string_view port);
private:
    void SetupSignalHandlers();
    static void SignalHandler(int s);
    int GetIntPort(std::string_view port);
    void InitServer(int port);
    void MainLoop();

    static volatile std::atomic<bool> g_terminated;
    std::unique_ptr<TCPUPDServer> m_server;
    mutable boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level> m_logger;
};