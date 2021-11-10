#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>

#define DO_SYS(syscall, syscall_name)                  \
  do                                                   \
  {                                                    \
    if ((syscall) == -1)                               \
    {                                                  \
      string str_for_perror = string("smash error: "); \
      str_for_perror += syscall_name;                  \
      str_for_perror += " failed";                     \
      perror((char *)str_for_perror.c_str());          \
      return;                                          \
    }                                                  \
  } while (0)

using namespace std;

void ctrlZHandler(int sig_num) {
  cout<< "smash: got ctrl-Z" <<endl;
  SmallShell& smash = SmallShell::getInstance();
  Command* cmd = smash.GetCommand();
  if (!cmd){  
    return;
  }
  if (cmd->isForeground()){
    DO_SYS(kill(cmd->GetPid() , SIGSTOP), "kill");
    cout<< "smash: process " << cmd->GetPid() << " was stopped" <<endl;
    cmd->SetForeground(false);
  }
}

void ctrlCHandler(int sig_num) {
  cout<< "smash: got ctrl-C" <<endl;
  SmallShell& smash = SmallShell::getInstance();
  Command* cmd = smash.GetCommand();
  if (!cmd){  
    return;
  }
  if (cmd->isForeground()){
    DO_SYS(kill(cmd->GetPid(), SIGKILL), "kill");
    cout<< "smash: process " << cmd->GetPid() << " was killed" <<endl;
  }
}

void alarmHandler(int sig_num) {
  cout<< "smash: got an alarm" <<endl;
  SmallShell& smash = SmallShell::getInstance();
  TimesList& times_ref = smash.GetTimesListReference();
  times_ref.killFinishedAlarms();
}