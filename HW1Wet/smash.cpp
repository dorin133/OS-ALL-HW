#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

#define RUN 1

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    struct sigaction act={0};
    // sigemptyset (&act.sa_mask);
    act.sa_flags = SA_RESTART;
    act.sa_handler = &alarmHandler;
    if(sigaction(SIGALRM, &act, NULL) ==-1) {
        perror("smash error: failed to set SigAlarm handler");
    }

    // alarm(1);

    SmallShell& smash = SmallShell::getInstance();
    while(smash.GetRun() == RUN) {
        std::cout << smash.GetPrompt();
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.RemoveFinishedJobs();
        smash.executeCommand(cmd_line.c_str());
        if (smash.GetCommand())
            smash.GetCommand()->SetForeground(false);
        // cout << "bye" << endl;
        // smash.RemoveFinishedJobs();
        // smash.printJobsList();
    }
    return 0;
}