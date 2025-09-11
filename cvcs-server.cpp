#include <iostream>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>

#include "networkUtils.h"
#include "utils.h"

// General logging functions
template <typename T>
void error(T errMessage){
    std::cout << "[-] " << errMessage << std::endl;
}

template <typename T>
void log(T message){
    std::cout << "[!] " << message << std::endl;
}

std::string convertToServerPath(std::string projectName, std::string filePath){
    // /home/cupycake/Desktop/cvcs/testRepo/folder/main.cpp

    // TODO: PLEASE DO THIS
}

int main(int argc, char* argv[]){
    log("Checking if projects directory exists...");
    
    bool projectsFound = false;
    for(auto file : std::filesystem::directory_iterator(std::filesystem::current_path())){
        std::string fileName = file.path().filename().string();

        if(fileName == "projects"){
            projectsFound = true;
        }
    }

    if(!projectsFound){
        error("Projects folder not found!! :c");
        return -1;
    }

    int serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);

    char* ip = argv[1];

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2956);
    inet_pton(AF_INET, ip, &serverAddress.sin_addr);

    bind(serverSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    listen(serverSocketFD, 10);

    log("Waiting for connections on " + std::string(ip) + ":2956");

    int clientSocketFD = accept(serverSocketFD, nullptr, nullptr);

    std::string command = receiveMessage(clientSocketFD);

    if(command == "upload"){
        std::string projectName = receiveMessage(clientSocketFD);
        std::string filePath = receiveMessage(clientSocketFD);
        std::string uploadedContent = receiveMessage(clientSocketFD);
        
        log(filePath + " from project " + projectName + " has been uploaded");
        log(uploadedContent);

        // Check if project exists
        bool foundProject = false;
        for(auto file : std::filesystem::directory_iterator("projects")){
            if(file.path().filename().string() == projectName){
                foundProject = true;
            }
        }

        if(foundProject){
            // Convert to path on server filesystem
            std::string serverPath = convertToServerPath(projectName, filePath);

            // Check if file exists
            if(doesFileExist(filePath)){

            }

            // If it does, find differences between current save and uploaded content

            // Apply differences

            // If it doesn't, create file with that content
        }else{
            // Create project folder

            // 
        }

    }else if(command == "download"){
        std::string projectName = receiveMessage(clientSocketFD);

        // Search for project and send over all project files
    
    }else if(command == "list"){
        // dummy for now
        sendMessage(clientSocketFD, "3");
        sendMessage(clientSocketFD, "project1");
        sendMessage(clientSocketFD, "project2");
        sendMessage(clientSocketFD, "project3");
    }

    close(serverSocketFD);

    return 0;
}