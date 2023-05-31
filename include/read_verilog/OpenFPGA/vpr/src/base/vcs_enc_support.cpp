#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "vcs_enc_support.h"


struct CommandResult {
    int exitCode;
    std::string output;
};

CommandResult executeCommand(const std::string& command) {
    CommandResult result;
    char buffer[128];
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe) {
        try {
            while (!feof(pipe)) {
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result.output += buffer;
                }
            }
            result.exitCode = pclose(pipe) >> 8;
        } catch (...) {
            pclose(pipe);
            throw;
        }
    } else {
        throw std::runtime_error("Failed to execute shell command.");
    }
    return result;
}

int commands(const std::string& file_to_process, const std::string& path_to_file) {

// check if user has docker and our image

    try {
        std::string shellCommand = "docker images rs_vcs_support | grep rs_vcs_support";
        CommandResult result = executeCommand(shellCommand);
        std::cout << "Exit code: " << result.exitCode << std::endl;
        //std::cout << "Command status:\n" << result.output << std::endl;
        if (result.exitCode != 0) {
                return 1;  // Non-zero exit status
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error executing shell command: " << ex.what() << std::endl;
        return 1;  // Terminate the program with a non-zero exit status
    }

// Get the required environment variables
    const std::vector<const char*> envVariables = {
        "VCS_HOME",
        "LIC_IP",
        "LIC_PORT"
    };

    std::vector<std::string> envValues;

    for (const auto& envVar : envVariables) {
        if (const char* value = std::getenv(envVar)) {
            envValues.push_back(value);
            //std::cout << envVar << " = " << value << '\n';
        } else {
            std::cout << "Environment variable " << envVar  << " is expected but not found" << std::endl;
            return 1;
        }
    }

// get host user id and Gid values to be env values in container using commands

std::vector<std::string> commands = {"id -u","id -g" 
                                           };

    for (const auto& command : commands) {
        try {
            CommandResult result = executeCommand(command);
            //std::cout << "Command: " << command << std::endl;
            std::cout << "Exit code: " << result.exitCode << std::endl;
            //std::cout << "Command status:\n" << result.output << std::endl;

            if (result.exitCode != 0) {
                return 1;  // Non-zero exit status
            }
            else{
                result.output.pop_back(); // this to remove the last \n
                envValues.push_back(result.output);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Error executing shell command: " << ex.what() << std::endl;
            return 1;  // Non-zero exit status
        }
    }

// append the name of directory and file to process

    envValues.push_back(file_to_process);
    envValues.push_back(path_to_file);
/*
// print the values in case of debug
    for (const auto& value : envValues) {
        std::cout << value << '\n';
    }
*/

// invoke the docker
    try {
        std::string shellCommand = "docker run --rm -v ";
        shellCommand +=  envValues[0];
        shellCommand +=  ":/opt/tool -v ";
        shellCommand +=  envValues[6];
        shellCommand += ":/workdir  -e LIC_IP=";
        shellCommand +=  envValues[1];
        shellCommand += " -e HOST_USER_ID=";
        shellCommand += envValues[3];
        shellCommand += " -e LIC_PORT=";
        shellCommand += envValues[2];
        shellCommand += " -e ENC_FILENAME=";
        shellCommand += envValues[5];
        shellCommand += " -e HOST_GROUP_ID=";
        shellCommand += envValues[4];
        shellCommand += " rs_vcs_support:0.1";
        //std::cout << "docker command is \n" << shellCommand << std::endl;
        //return 1;
        CommandResult result = executeCommand(shellCommand);
        std::cout << "Exit code: " << result.exitCode << std::endl;
        //std::cout << "Command status:\n" << result.output << std::endl;
        if (result.exitCode != 0) {
                return 1;  // Non-zero exit status
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error executing shell command: " << ex.what() << std::endl;
        return 1;  // Terminate the program with a non-zero exit status
    }



    return 0;  // Zero exit status

}


