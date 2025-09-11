#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

// Function for checking if a file exists
// filePath -> path to the file to check
bool doesFileExist(std::string filePath);

// Function for getting changes between two files (line-by-line, update to Myer's diff pls and thanks)
// file1 -> the first file's contents
// file2 -> the second file's contents
std::vector<std::string> getChanges(std::string file1Contents, std::string file2Contents);

#endif