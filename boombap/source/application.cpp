//
// Created by Javier Andoni Aizpuru Rodríguez on 15/09/26.
//

#include "application.h"
#include "processors/processors.h"

application::application()
{
    add_commands();
}

void application::add_commands()
{
    addHelpCommand("--help|-h", "Usage:", true);
    addVersionCommand("--version|-v",APPLICATION_VERSION);
    addCommand(get_process_command());
}

juce::ConsoleApplication::Command application::get_process_command()
{
    juce::ConsoleApplication::Command command;

    command.argumentDescription = "-f filename_to_master filename_of_master";
    command.commandOption = "--file|-f";
    command.command = [] (const auto& args)
    {
        const auto arguments = args.arguments;
        const auto size = arguments.size();

        if (size == 3 || size == 4)
        {
            const auto file = arguments[1].resolveAsFile();
            const auto output_file = arguments[2].resolveAsFile();

            if (file.existsAsFile())
            {
                if (master_processor::master_audio_file(file, output_file))
                {
                    std::cout << output_file.getFullPathName() << std::endl;
                }
            }
        }
    };

    return command;
}
