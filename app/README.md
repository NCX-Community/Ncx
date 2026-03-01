
# Ncx
A lightweight, stable and high-performance intranet penetration tool. Like frp, Ncx is desighed to help users easily expose local services to the public network.
## Content
- [Ncx](https://github.com/NCX-Community/Ncx/edit/main/README.md#ncx)
- [Features](https://github.com/NCX-Community/Ncx/edit/main/README.md#features)
- [Usage / QuickStart](https://github.com/NCX-Community/Ncx/edit/main/README.md#usage--quickstart)
  - [Configuration](https://github.com/NCX-Community/Ncx/edit/main/README.md#configuration)
- [Installation & Build](https://github.com/NCX-Community/Ncx/edit/main/README.md#installation--build)
- [Contribute](https://github.com/NCX-Community/Ncx/edit/main/README.md#contribute)
- [Planning](https://github.com/NCX-Community/Ncx/edit/main/README.md#planning)

## Features
- **High Performance**: Uses an asynchronous event-driven underlying network framework as the engine, providing non-blocking and instant response for high performance Uses an efficient network programming model to ensure low latency and high throughput
- **Protocol Support**: Currently supports only TCP
- **Low Overhead**: Binary size is only ~450KB, making it suitable for embedded devices such as routers or low-spec cloud servers
- **Easy Deployment**: Provides a simple installation and usage method for quickly setting up intranet penetration services
## Usage / QuickStart
The precompiled binary of ncx is available on the release page, or you can follow the complete compilation steps below to build it for your target platform.

The usage of ncx is quite similar to frp. If you have experience with similar tools, configuring ncx will be an easier process for you.

To use ncx, you need a server with a public IP (such as a cloud server) and a device running the service you want to expose to the public network. The cloud server can usually be configured with minimal specifications since ncx is lightweight. This allows you to enjoy high-performance services on the public network without incurring high costs for a powerful cloud server.

Before starting, you need to create both server-side and client-side configuration files in TOML format.
### Configuration
- **Client Configuration**

The client configuration file should also be written in TOML format. Below is an example configuration:

```toml
[client]
server_ip = "127.0.0.1"
server_port = 6666

[service_name]
service_ip = "127.0.0.1"
service_port = 22
proxy_port = 6667
```

- `server_ip`: The IP address of the server to connect to.
- `server_port`: The port on which the server is listening.
- `service_ip`: The IP address of the local service to expose.
- `service_port`: The port of the local service to expose.
- `proxy_port`: The port on which the proxy will be available on the server.

Before starting the client, ensure that the configuration file is properly set up.

- **Server Configuration**

The server configuration file should be written in TOML format. Below is an example configuration:

```toml
[server]
server_ip = "127.0.0.1"
server_port = 6666
backlog = 256
```

- `server_ip`: The IP address the server will bind to.
- `server_port`: The port on which the server will listen.
- `backlog`: The maximum number of pending connections in the queue.

Before starting the service, ensure that the configuration file is properly set up.

**Note：**

The spelling of configuration items must be exactly the same as specified. Any deviation may result in errors and failure to recognize the settings.

**Program Startup Scipt**
- Server
```shell
./ncxs ncxs.toml
```
- Client
```shell
./ncxc ncxc.toml
```

All the progarm can find in release page.

## Installation & Build
1. Dependency Installation

Before building the project, ensure that you have the following dependencies installed:

- protobuf (`libprotobuf-dev`, `protobuf-compiler`)
- boost (`libboost-all-dev`)
- libevent (`libevent-dev`)

2. Compilation

Follow these steps to compile the project:

1. Create a build folder in the root directory.
2. Navigate to the build folder.
3. Run cmake .. to generate the build configuration.
4. Run make to compile the project.

```shell
mkdir build && cd build
cmake ..
make
```

## Contribute
We welcome all high-quality issues and pull requests. This project is still in its early stages, and there is a lot of work to be done.

Intranet penetration tools have significant performance requirements, and C++ allows us to push performance to its limits.

Contributions are welcome! Feel free to submit pull requests or open issues.
## Planning
- [ ] UDP Support
- [ ] Http Support
- [ ] configure file hot reload
- [x] Docker Start Support
- [ ] High availability Support
