#include <iostream>
#include "cli.h"
#include <event2/thread.h>

const std::string DRAW_C = R"(

#   __      _____ _    ___ ___  __  __ ___   _____ ___  
#   \ \    / / __| |  / __/ _ \|  \/  | __| |_   _/ _ \ 
#    \ \/\/ /| _|| |_| (_| (_) | |\/| | _|    | || (_) |
#     \_/\_/ |___|____\___\___/|_|  |_|___|   |_| \___/ 
#                                                       
#    _  _  _____  _____ _    ___ ___ _  _ _____         
#   | \| |/ __\ \/ / __| |  |_ _| __| \| |_   _|        
#   | .` | (__ >  < (__| |__ | || _|| .` | | |          
#   |_|\_|\___/_/\_\___|____|___|___|_|\_| |_|          
#                                                       

)";

int
main(int argc, char* argv[])
{
    // Must be called before any EventLoop (event_base_new) is created.
    evthread_use_pthreads();

    std::cout << DRAW_C << std::endl;

    // cli parse
    // cli是客户端程序的入口
    Cli::Parse(argc, argv);
}