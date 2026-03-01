#include <boost/program_options.hpp>
#include <iostream>
#include <stdexcept>  // 包含标准异常类型

#include "singleton.h"
#include "nclient.h"
#include "ccontrolchannel.h"
#include "config.h"

namespace po = boost::program_options;

class Cli : public Singleton<Cli> {
public:
    /// @brief 使用命令行传入的参数解析
    /// @param argc 参数数量
    /// @param argv 参数数组
    static void Parse(int argc, char* argv[]) {
        try {
            GetInstance().parse(argc, argv);
        } catch (const std::exception& e) {
            // 捕获所有未被处理的异常
            std::cerr << "Fatal Error: " << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
    }

protected:
    void parse(int argc, char* argv[]) {
        try {
            // 定义命令行选项
            desc.add_options()
                ("version, v", "Print version information")
                ("help, h", "Print help message")
                ("local, l", po::value<std::string>(), "Path to local config file")
                ("remote, r", po::value<std::string>(), "Configure remote tunnel via web")
            ;

            // 解析命令行参数
            po::store(po::parse_command_line(argc, argv, desc), vm);
            po::notify(vm);

            // 处理帮助和版本信息
            if (vm.count("help")) {
                printHelp();
            } else if (vm.count("version")) {
                printVersion();
            } else if (vm.count("local")) {
                handleLocalConfig();
            } else if (vm.count("remote")) {
                handleRemoteTunnel();
            } else {
                throw std::runtime_error("No valid command specified. Use --help for options.");
            }
        } catch (const po::error& e) {
            // 捕获 Boost 命令行解析错误
            throw std::runtime_error("Command line error: " + std::string(e.what()));
        }
    }

private:
    po::options_description desc {"Allowed options"};
    po::variables_map vm;

    // 辅助函数：打印帮助信息
    void printHelp() {
        std::cout << desc << "\n";
        exit(EXIT_SUCCESS);
    }

    // 辅助函数：打印版本信息
    void printVersion() {
        std::cout << "Version 1.0.0\n";
        exit(EXIT_SUCCESS);
    }

    // 处理本地配置文件
    void handleLocalConfig() {
        try {
            std::string local_file = vm["local"].as<std::string>();
            std::unique_ptr<ClientConfig> config;

            // 加载配置文件
            try {
                config = std::make_unique<ClientConfig>(local_file);
            } catch (const std::exception& e) {
                throw std::runtime_error("Failed to load config file [" + local_file + "]: " + e.what());
            }

            // 解析配置并启动客户端
            try {
                auto args = config->parseAsControlChannelArgsVec();
                std::unique_ptr<NClient> client = std::make_unique<NClient>(args);
                client->run_client();
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Client initialization failed: ") + e.what());
            }
        } catch (const std::exception& e) {
            throw; // 重新抛出给外层处理
        }
    }

    // 处理远程隧道（示例占位符）
    void handleRemoteTunnel() {
        std::string remote_url = vm["remote"].as<std::string>();
        throw std::runtime_error("Remote tunnel not implemented. URL: " + remote_url);
    }
};