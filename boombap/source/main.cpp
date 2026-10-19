#include "application.h"

int main(int argc, char* argv[])
{
    const application app;

    return app.findAndRunCommand(argc, argv);
}