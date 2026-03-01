#include <toml++/toml.hpp>
#include <iostream>
#define TOML_EXCEPTIONS 0 

int main()
{
    const char *path = "example_client.toml";
    toml::parse_result config = toml::parse_file(path);
    if (config.empty())
    {
        throw std::runtime_error("Failed To Parse Client Config File!");
    }
    toml::table *table = config.as_table();
    if (!table->at_path("client"))
    {
        throw std::runtime_error("Failed To Parse Client Config File! [ client didn't exist !]");
        if (!table->at_path("client.server_ip"))
        {
            throw std::runtime_error("Failed To Parse Client Config File! [ server_ip didn't exist !]");
        }
        if (!table->at_path("client.server_port"))
        {
            throw std::runtime_error("Failed To Parse Client Config File! [ server_port didn't exist !]");
        }
    }
    std::string server_ip = table->at_path("client.server_ip").as_string()->get();
    int server_port = table->at_path("client.server_port").as_integer()->get();
    std::cout << "server_ip: " << server_ip << "server_port: " << server_port << std::endl;

    // parse all client service
    table->for_each([](const toml::key &key, auto val)
                    {
            if(key == "client")
            {
                return;
            }
            auto service_name = key.data();
            std::string service_ip = val.as_table()->at_path("service_ip").as_string()->get();
            int service_port = val.as_table()->at_path("service_port").as_integer()->get();
            int proxy_port = val.as_table()->at_path("proxy_port").as_integer()->get();
            std::cout << "service_name: " << service_name << "service_ip: " << service_ip << " service_port: " << service_port << " proxy_port: " << proxy_port << std::endl;
            });
}