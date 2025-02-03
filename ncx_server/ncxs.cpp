#include "nserver.h"
#include "InetAddress.h"
#include "config.h"
#include <iostream>

const std::string DRAW_S = R"(  
#   __      _____ _    ___ ___  __  __ ___   _____ ___  
#   \ \    / / __| |  / __/ _ \|  \/  | __| |_   _/ _ \ 
#    \ \/\/ /| _|| |_| (_| (_) | |\/| | _|    | || (_) |
#     \_/\_/ |___|____\___\___/|_|  |_|___|   |_| \___/ 
#                                                       
#    _  _  _____  _____ ___ _____   _____ ___           
#   | \| |/ __\ \/ / __| __| _ \ \ / / __| _ \          
#   | .` | (__ >  <\__ \ _||   /\ V /| _||   /          
#   |_|\_|\___/_/\_\___/___|_|_\ \_/ |___|_|_\          
#                                                       
)";


int 
main(int argc, char* argv[])
{
    std::cout << DRAW_S << std::endl;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    
    // read server config 
    ServerConfig config(argv[1]);
    NServer ncxs(config.parseAsServerArgs());
    ncxs.run_server();
}