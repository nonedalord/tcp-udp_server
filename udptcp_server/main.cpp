#include "Application.h"

int main(int argc, char* argv[])
{
    Application app;
    if (argc > 1)
    {
        return app.Run(argv[1]);
    }
    else 
    {
        return app.Run("8087");
    }
}