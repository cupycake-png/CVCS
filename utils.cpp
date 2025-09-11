#include <sstream>
#include <filesystem>

#include "utils.h"

bool doesFileExist(std::string filePath){
    return std::filesystem::exists(filePath);
}

std::vector<std::string> getChanges(std::string file1Contents, std::string file2Contents){
    std::vector<std::string> changes = {};

    // If either are empty, return the other as a change

    if(file1Contents == ""){
        // Return the entirety of file2 as a change
        std::istringstream file2Stream(file2Contents);
        
        std::string line;
        int counter = 0;
        while(std::getline(file2Stream, line)){
            counter++;
            changes.push_back(std::to_string(counter) + ":" + line);
        }

        return changes;
    }

    if(file2Contents == ""){
        // Return the entirety of file1 as a change
        std::istringstream file1Stream(file1Contents);

        std::string line;
        int counter = 0;
        while(std::getline(file1Stream, line)){
            counter++;
            changes.push_back(std::to_string(counter) + ":" + line);
        }

        return changes;
    }

    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;

    std::istringstream oldStream(file1Contents);
    std::string line;

    while(std::getline(oldStream, line)){
        oldLines.push_back(line);
    }

    std::istringstream newStream(file2Contents);
    while(std::getline(newStream, line)){
        newLines.push_back(line);
    }

    // Use the biggest size to avoid.. issues
    size_t maxLines = std::max(oldLines.size(), newLines.size());

    for(size_t i=0; i<maxLines; i++){
        std::string oldLine = (i < oldLines.size() ? oldLines[i] : "");
        std::string newLine = (i < newLines.size() ? newLines[i] : "");

        if(oldLine != newLine){
            changes.push_back(std::to_string(i+1) + ":" + newLine);
        }
    }

    return changes;
}
