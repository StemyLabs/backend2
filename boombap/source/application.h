//
// Created by Javier Andoni Aizpuru Rodríguez on 15/09/26.
//

#ifndef MATERINGSERVICE_APPLICATION_H
#define MATERINGSERVICE_APPLICATION_H

#include <juce_core/juce_core.h>

class application : public juce::ConsoleApplication
{
public:
    application();
private:

    void add_commands();
    juce::ConsoleApplication::Command get_process_command();
};


#endif //MATERINGSERVICE_APPLICATION_H
