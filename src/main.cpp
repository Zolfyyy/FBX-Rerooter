#include <CLI/CLI.hpp>
#include "commands.hpp"

int main(int argc, char **argv)
{
    CLI::App app{"FBX armature re-rooting utility"};

    CommandContext context;
    CommandRegistry::instance().setup_all(app, context);
    app.set_version_flag("--version", PROJECT_VERSION, "Display the program version");
    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}