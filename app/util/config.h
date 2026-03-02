#include <toml++/toml.hpp>
#include "util.h"
#include "InetAddress.h"
#include "singleton.h"
#include "nserver.h"
#include "nclient.h"
#include "engine_proxy_config.h"

// #define TOML_EXCEPTIONS 0

class Config
{
public:
    Config() = delete;
    Config(const std::string& path)
    {
        config = toml::parse_file(path);
    }
    ~Config() = default;
protected:
    toml::parse_result config;
};

class ServerConfig : public Config
{
public:
    ServerConfig() = delete;
    ServerConfig(const std::string& path) : Config(path) {}
    ~ServerConfig() = default;

    ServerArgs parseAsServerArgs() {
        if (config.empty()) {
            throw std::runtime_error("Failed To Parse Server Config File: Config is empty!");
        }
    
        try {
            // 获取根表
            toml::table* table = config.as_table();
            if (!table) {
                throw std::runtime_error("Failed To Parse Server Config File: Config is not a valid table!");
            }
    
            // 检查 server 节点是否存在
            auto server_node = table->at_path("server");
            if (!server_node || !server_node.is_table()) {
                throw std::runtime_error("Failed To Parse Server Config File: [server] section is missing or invalid!");
            }
    
            auto server_table = server_node.as_table();
    
            // 检查 server_ip 是否存在且为字符串
            auto server_ip_node = server_table->at_path("server_ip");
            if (!server_ip_node || !server_ip_node.is_string()) {
                throw std::runtime_error("Failed To Parse Server Config File: [server.server_ip] is missing or invalid!");
            }
    
            // 检查 server_port 是否存在且为整数
            auto server_port_node = server_table->at_path("server_port");
            if (!server_port_node || !server_port_node.is_integer()) {
                throw std::runtime_error("Failed To Parse Server Config File: [server.server_port] is missing or invalid!");
            }
    
            // 检查 backlog 是否存在且为整数
            auto backlog_node = server_table->at_path("backlog");
            if (!backlog_node || !backlog_node.is_integer()) {
                throw std::runtime_error("Failed To Parse Server Config File: [server.backlog] is missing or invalid!");
            }
    
            // 解析具体值
            std::string server_ip = server_ip_node.as_string()->get();
            int server_port = server_port_node.as_integer()->get();
            int backlog = backlog_node.as_integer()->get();
    
            // 返回 ServerArgs 结构体
            return ServerArgs{
                InetAddress(server_ip.c_str(), static_cast<uint16_t>(server_port)),
                backlog
            };
        } catch (const std::exception& e) {
            // 捕获所有可能的异常，并输出错误信息
            std::cerr << "Error: " << e.what() << std::endl;
            exit(1); // 退出程序，避免继续执行无效状态
        }
    }

    /// Parse the optional [engine_proxy] section.
    /// Returns std::nullopt if the section is absent (standalone / backward-compatible mode).
    std::optional<EngineProxyConfig> parseEngineProxyConfig() {
        try {
            toml::table* table = config.as_table();
            if (!table) return std::nullopt;

            auto ep_node = table->at_path("engine_proxy");
            if (!ep_node || !ep_node.is_table()) return std::nullopt;

            auto ep_table = ep_node.as_table();

            auto endpoint_node = ep_table->at_path("proxy_endpoint");
            if (!endpoint_node || !endpoint_node.is_string())
                throw std::runtime_error("[engine_proxy.proxy_endpoint] is missing or not a string");

            auto engine_id_node = ep_table->at_path("engine_id");
            if (!engine_id_node || !engine_id_node.is_string())
                throw std::runtime_error("[engine_proxy.engine_id] is missing or not a string");

            auto engine_addr_node = ep_table->at_path("engine_address");
            if (!engine_addr_node || !engine_addr_node.is_string())
                throw std::runtime_error("[engine_proxy.engine_address] is missing or not a string");

            auto max_cap_node = ep_table->at_path("max_capacity");
            if (!max_cap_node || !max_cap_node.is_integer())
                throw std::runtime_error("[engine_proxy.max_capacity] is missing or not an integer");

            EngineProxyConfig cfg;
            cfg.proxy_endpoint  = endpoint_node.as_string()->get();
            cfg.engine_id       = engine_id_node.as_string()->get();
            cfg.engine_address  = engine_addr_node.as_string()->get();
            cfg.max_capacity    = static_cast<int32_t>(max_cap_node.as_integer()->get());

            auto hb_node = ep_table->at_path("heartbeat_interval");
            if (hb_node && hb_node.is_integer())
                cfg.heartbeat_interval = static_cast<int>(hb_node.as_integer()->get());

            return cfg;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing [engine_proxy]: " << e.what() << std::endl;
            exit(1);
        }
    }
};

class ClientConfig : public Config
{
public:
    ClientConfig() = delete;
    ClientConfig(const std::string& path) : Config(path) {}
    ~ClientConfig() = default;

    std::vector<CControlChannelArgs> parseAsControlChannelArgsVec() {
        try {
            std::vector<CControlChannelArgs> result;
    
            // 检查配置文件是否为空
            if (config.empty()) {
                throw std::runtime_error("Failed To Parse Client Config File: Config is empty!");
            }
    
            // 获取根表
            toml::table* table = config.as_table();
            if (!table) {
                throw std::runtime_error("Failed To Parse Client Config File: Config is not a valid table!");
            }
    
            // 检查 client 节点是否存在
            auto client_node = table->at_path("client");
            if (!client_node || !client_node.is_table()) {
                throw std::runtime_error("Failed To Parse Client Config File: [client] section is missing or invalid!");
            }
    
            auto client_table = client_node.as_table();
    
            // 检查 server_ip 是否存在且为字符串
            auto server_ip_node = client_table->at_path("server_ip");
            if (!server_ip_node || !server_ip_node.is_string()) {
                throw std::runtime_error("Failed To Parse Client Config File: [client.server_ip] is missing or invalid!");
            }
    
            // 检查 server_port 是否存在且为整数
            auto server_port_node = client_table->at_path("server_port");
            if (!server_port_node || !server_port_node.is_integer()) {
                throw std::runtime_error("Failed To Parse Client Config File: [client.server_port] is missing or invalid!");
            }
    
            // 解析 server_ip 和 server_port
            std::string server_ip = server_ip_node.as_string()->get();
            int server_port = server_port_node.as_integer()->get();
            InetAddress server_addr(server_ip.c_str(), static_cast<uint16_t>(server_port));
    
            // 解析所有客户端服务
            table->for_each([server_addr, &result](const toml::key& key, auto val) {
                if (key == "client") {
                    return; // 跳过 client 节点
                }
    
                // 检查当前节点是否为表
                auto service_table = val.as_table();
                if (!service_table) {
                    throw std::runtime_error("Failed To Parse Client Config File: Service [" + std::string(key) + "] is not a valid table!");
                }
    
                // 检查 service_ip 是否存在且为字符串
                auto service_ip_node = service_table->at_path("service_ip");
                if (!service_ip_node || !service_ip_node.is_string()) {
                    throw std::runtime_error("Failed To Parse Client Config File: [service_ip] is missing or invalid in service [" + std::string(key) + "]!");
                }
    
                // 检查 service_port 是否存在且为整数
                auto service_port_node = service_table->at_path("service_port");
                if (!service_port_node || !service_port_node.is_integer()) {
                    throw std::runtime_error("Failed To Parse Client Config File: [service_port] is missing or invalid in service [" + std::string(key) + "]!");
                }
    
                // 检查 proxy_port 是否存在且为整数
                auto proxy_port_node = service_table->at_path("proxy_port");
                if (!proxy_port_node || !proxy_port_node.is_integer()) {
                    throw std::runtime_error("Failed To Parse Client Config File: [proxy_port] is missing or invalid in service [" + std::string(key) + "]!");
                }
    
                // 解析具体值
                std::string service_ip = service_ip_node.as_string()->get();
                int service_port = service_port_node.as_integer()->get();
                int proxy_port = proxy_port_node.as_integer()->get();
    
                // 构造 CControlChannelArgs 并添加到结果中
                result.push_back(CControlChannelArgs{
                    server_addr,
                    key.data(),
                    InetAddress(service_ip.c_str(), static_cast<uint16_t>(service_port)),
                    static_cast<uint16_t>(proxy_port)
                });
            });
    
            return result;
        } catch (const std::exception& e) {
            // 捕获所有可能的异常，并输出错误信息
            std::cerr << "Error: " << e.what() << std::endl;
            exit(1); // 退出程序，避免继续执行无效状态
        }
    }
};