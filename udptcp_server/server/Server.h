#pragma once

#include <thread>
#include <set>
#include <shared_mutex>
#include <atomic>
#include <string_view>
#include <condition_variable>
#include <mutex>

#include "ThreadPoolQueue.h"
#include "../logging/Logging.h"

class TCPUPDServer 
{
public:
    using ShutdownCallback = std::function<void()>;
    TCPUPDServer();
    ~TCPUPDServer();
    void Init(int port, unsigned int max_events = 64);
    void ListenAsync(unsigned int max_threads = 4);
    void SetShutdownCallback(ShutdownCallback&& callback);
    void Stop();
private:
    void AddSocketToEpoll(unsigned int client_socket, uint32_t events);
    void HandleNewTCPConnection();
    void HandleTCPClientData(unsigned int client_socket);
    void HandleUDPData();
    void CloseSocket(unsigned int client_socket);
    std::string PrepareAnswer(std::string_view);
    
    std::mutex m_shutdown_mutex;
    std::condition_variable m_shutdown_cv;
    std::thread m_shutdown_thread;
    std::mutex m_callback_mutex;
    ShutdownCallback m_shutdown_callback;
    std::atomic<bool> m_is_shutdown;
    std::atomic<unsigned int> m_clients_count;
    std::shared_mutex m_set_mutex;
    int m_tcp_socket;
    int m_udp_socket;
    std::thread m_server_thread;
    std::set<unsigned int> m_client_sockets;
    int m_epoll_fd;
    std::atomic<bool> m_server_run;
    int m_shutdown_event_fd;
    unsigned int m_max_events;
    const uint32_t m_error_mask;
    std::unique_ptr<ThreadPoolQueue> m_task_queue;
    mutable boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level> m_logger;
};