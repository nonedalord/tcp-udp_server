#include "Server.h"

#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>

TCPUPDServer::TCPUPDServer() : m_server_run(false), m_tcp_socket(-1), m_udp_socket(-1), m_epoll_fd(-1), m_error_mask(EPOLLHUP | EPOLLERR | EPOLLRDHUP),
m_shutdown_event_fd(-1), m_is_shutdown(false), m_task_queue(std::make_unique<ThreadPoolQueue>()), m_logger(boost::log::keywords::channel = "Server") {}

TCPUPDServer::~TCPUPDServer()
{
    Stop();
    m_is_shutdown.store(true);
    m_shutdown_cv.notify_one();
    if (m_shutdown_thread.joinable())
    {
        m_shutdown_thread.join();
    }
}

void TCPUPDServer::Stop()
{
    if (!m_server_run)
    {
        return;
    }
    m_server_run.store(false);
    uint64_t value = 1;
    write(m_shutdown_event_fd, &value, sizeof(value));
    close(m_udp_socket);
    for (auto& client_socket : m_client_sockets) 
    {
        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, client_socket, nullptr);
        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
    }
    close(m_tcp_socket);
    close(m_epoll_fd);
    close(m_shutdown_event_fd);
    if (m_server_thread.joinable())
    {
        m_server_thread.join();
    }
    m_task_queue->Stop();
    LOG(m_logger, LogHelper::info, "Server closed");
}

void TCPUPDServer::Init(int port, unsigned int max_events)
{
    m_max_events = max_events;
    m_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    m_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (m_tcp_socket < 0 || m_udp_socket < 0) 
    {
        throw std::runtime_error("socket creating error");
    }
    int reuse = 1;

    if (setsockopt(m_tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) 
    {
        close(m_tcp_socket);
        close(m_udp_socket);
        throw std::runtime_error("reuse failed");
    }

    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd < 0)
    {
        close(m_tcp_socket);
        close(m_udp_socket);
        throw std::runtime_error("epoll creation error");
    }

    m_shutdown_event_fd = eventfd(0, EFD_NONBLOCK);
    AddSocketToEpoll(m_shutdown_event_fd, EPOLL_CTL_ADD);

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(m_tcp_socket, (sockaddr*)&address, sizeof(address)) < 0 || bind(m_udp_socket, (sockaddr*)&address, sizeof(address)) < 0)
    {
        throw std::runtime_error("bind address error");
    }

    if (listen(m_tcp_socket, 3) < 0)
    {
        throw std::runtime_error("listen tcp error");
    }

    int flags = fcntl(m_tcp_socket, F_GETFL, 0);
    fcntl(m_tcp_socket, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(m_udp_socket, F_GETFL, 0);
    fcntl(m_udp_socket, F_SETFL, flags | O_NONBLOCK);

    AddSocketToEpoll(m_tcp_socket, EPOLLIN | EPOLLET);
    AddSocketToEpoll(m_udp_socket, EPOLLIN);

    LOG(m_logger, LogHelper::info, "Server started on port " << port);
}

void TCPUPDServer::AddSocketToEpoll(unsigned int client_socket, uint32_t events)
{
    epoll_event event;
    event.events = events;
    event.data.fd = client_socket;
    
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1) 
    {
        throw std::runtime_error("epoll_ctl add error");
    }
}

void TCPUPDServer::ListenAsync(unsigned int max_threads)
{
    m_server_run = true;
    
    m_task_queue->starAsync(max_threads);
    m_server_thread = std::thread([this] {
        epoll_event events[m_max_events];
        while(m_server_run.load())
        {
            int num_events = epoll_wait(m_epoll_fd, events, m_max_events, -1);
            if (num_events == -1) 
            {
                LOG(m_logger, LogHelper::error, "Error while epoll wait");
                continue;
            }

            for (int i = 0; i < num_events; ++i) 
            {
                if (!m_server_run.load())
                {
                    break;
                }
                int fd = events[i].data.fd;
                uint32_t event_flag = events[i].events;
                if (event_flag & (m_error_mask))
                {
                    if (fd != m_tcp_socket && fd != m_udp_socket && fd != m_shutdown_event_fd) 
                    {
                        CloseSocket(fd);
                        continue;
                    }
                }

                if (fd == m_tcp_socket)
                {
                    m_task_queue->Push(std::bind(&TCPUPDServer::HandleNewTCPConnection, this));
                } 
                else if (fd == m_udp_socket) 
                {
                    m_task_queue->Push(std::bind(&TCPUPDServer::HandleUDPData, this));
                }
                else if (fd == m_shutdown_event_fd)
                {
                    uint64_t value;
                    read(m_shutdown_event_fd, &value, sizeof(value));
                    break;
                }
                else 
                {
                    {
                        std::shared_lock lock(m_set_mutex);
                        auto it = m_client_sockets.find(fd);
                        if (it == m_client_sockets.end())
                        {
                            LOG(m_logger, LogHelper::warning, "Unknow descriptor " << fd);
                            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                            continue;
                        }
                    }
                    m_task_queue->Push(std::bind(&TCPUPDServer::HandleTCPClientData, this, fd));
                }
            }
        }
    });
}

void TCPUPDServer::HandleNewTCPConnection()
{
    while (m_server_run.load())
    {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(m_tcp_socket, (sockaddr*)&client_addr, &client_len);
        
        if (client_socket == -1) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                break;
            }
            else 
            {
                LOG(m_logger, LogHelper::error, "Error while accepting new tcp client: " << strerror(errno));
                break;
            }
        }
        
        int flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
        
        int enable_keepalive = 1;
        setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &enable_keepalive, sizeof(enable_keepalive));
        
        int do_nothing_sec = 30;
        int interval_sec = 5;
        int count = 3;
        
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPIDLE, &do_nothing_sec, sizeof(do_nothing_sec));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval_sec, sizeof(interval_sec));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
            
        AddSocketToEpoll(client_socket, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
        {
            std::unique_lock lock(m_set_mutex);
            m_client_sockets.insert(client_socket);
        }
        LOG(m_logger, LogHelper::info, "New TCP Connection " << client_socket);
        ++m_clients_count;
    }
}

void TCPUPDServer::HandleTCPClientData(unsigned int client_socket)
{
    if (!m_server_run.load()) 
    {
        return;
    }
    char buffer[m_buffer_size];
    ssize_t total_bytes_read = 0;
    bool is_closed = false;
    while (m_server_run.load())
    {
        ssize_t bytes_read = recv(client_socket, buffer + total_bytes_read, m_buffer_size - 1, 0);
        if (bytes_read > 0)
        {
            total_bytes_read += bytes_read;
            if (total_bytes_read >= m_buffer_size - 1)
            {
                total_bytes_read = m_buffer_size - 1;
                LOG(m_logger, LogHelper::warning, "Buffer is overflowed for client " << client_socket);
                break;
            }
        }
        else if (bytes_read == 0)
        {
            is_closed = true;
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else 
            {
                LOG(m_logger, LogHelper::error, "Reading error: " << strerror(errno) << " for client " << client_socket);
                is_closed = true;
                break;
            }
        }
    }

    if (!m_server_run.load())
    {
        return;
    }

    if (is_closed)
    {
        CloseSocket(client_socket);
        return;
    }

    if (total_bytes_read > 0)
    {
        buffer[total_bytes_read] = '\0';
        std::string_view message(buffer, total_bytes_read);
        LOG(m_logger, LogHelper::info, "New message from client " << client_socket << " : " << message);
        std::string response = PrepareAnswer(message);
        if (response.size() != 0)
        {
            if (send(client_socket, response.c_str(), response.size(), 0) == -1) 
            {
                LOG(m_logger, LogHelper::error, "Error while sending message " << response << " to TCP client " << client_socket);
            }
        }
    }
}

void TCPUPDServer::CloseSocket(unsigned int client_socket)
{
    close(client_socket);
    {
        std::unique_lock lock(m_set_mutex);
        m_client_sockets.erase(client_socket);
    }
    epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, client_socket, nullptr);
    LOG(m_logger, LogHelper::info, "Closed connection for client " << client_socket);
}

std::string TCPUPDServer::PrepareAnswer(std::string_view response)
{
    if (!response.starts_with("/"))
    {
        return response.data();
    }
    else
    {
        if (response == "/time")
        {
            LOG(m_logger, LogHelper::info, "Received time command");
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&time_t, &tm);
            
            char time_buffer[20];
            std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm);
            return std::string(time_buffer);
        }
        else if (response == "/stats")
        {
            LOG(m_logger, LogHelper::info, "Received stats command");
            return "Total clients: " + m_clients_count.Get() + ". Active clients: " + std::to_string(m_client_sockets.size());
        }
        else if (response == "/shutdown")
        {
            LOG(m_logger, LogHelper::info, "Received shutdown command");
            
            if (m_shutdown_callback)
            {
                m_is_shutdown.store(true);
                m_shutdown_cv.notify_all();
                {
                    std::unique_lock lock(m_callback_mutex);
                    m_shutdown_callback();
                }
            }
            return std::string();
        }
        else
        {
            LOG(m_logger, LogHelper::warning, "Received unknow command " << response);
            return "Unknow command";
        }
    }
}

void TCPUPDServer::HandleUDPData()
{
    if (!m_server_run.load()) 
    {
        return;
    }
    
    char buffer[1024];
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t bytes_recv = recvfrom(m_udp_socket, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &client_len);
    
    if (bytes_recv < 0) 
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        else 
        {
            LOG(m_logger, LogHelper::error, "UDP recvfrom error: " << strerror(errno));
            return;
        }
    }
    
    if (!m_server_run.load()) 
    {
        return;
    }

    buffer[bytes_recv] = '\0'; 
    std::string_view message(buffer, bytes_recv);
    LOG(m_logger, LogHelper::info, "Received message to UPD socket : " << message);
    
    if (bytes_recv > 0) 
    {
        std::string response = PrepareAnswer(message);
        if (response.size() != 0)
        {
            if (sendto(m_udp_socket, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, client_len) == -1)
            {
                LOG(m_logger, LogHelper::info, "Error while sending message " << response << " to UPD client ");
            }
        }
    }
}

void TCPUPDServer::SetShutdownCallback(ShutdownCallback&& callback)
{
    std::unique_lock lock(m_callback_mutex);
    m_shutdown_callback = std::forward<ShutdownCallback>(callback);
    static std::once_flag flag;
    std::call_once(flag, [this]() {
        m_shutdown_thread = std::thread([this]{
            while (!m_is_shutdown.load())
            {
                std::unique_lock lock(m_shutdown_mutex);
                m_shutdown_cv.wait(lock, [this] {
                    return m_is_shutdown.load();
                });
            }
            Stop();
        });
    });
}