#include "QtAppShell.h"

void QtAppShell::registerBootstrapCommands() {
    registerFileCommands();
    registerEditCommands();
    registerViewCommands();
    registerNavigationCommands();
    registerJournalCommands();
    registerToolCommands();
    registerHelpCommands();
}
