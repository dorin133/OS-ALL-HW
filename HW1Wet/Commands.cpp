#include <unistd.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <memory>
#include "Commands.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

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

#if 0
#define FUNC_ENTRY() \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \S
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

bool _isPipeCommand(string cmd_line, string &redirection_type)
{
  std::size_t found1 = cmd_line.find("|");
  std::size_t found2 = cmd_line.find("|&");
  if (found1 != std::string::npos && found2 == std::string::npos)
  {
    redirection_type = "|";
    return true;
  }
  if (found2 != std::string::npos)
  {
    redirection_type = "|&";
    return true;
  }
  return false;
}

bool _isRedirectionCommand(string cmd_line, string &pipe_type)
{
  std::size_t found1 = cmd_line.find(">");
  std::size_t found2 = cmd_line.find(">>");
  if (found1 != std::string::npos && found2 == std::string::npos)
  {
    pipe_type = ">";
    return true;
  }
  if (found2 != std::string::npos)
  {
    pipe_type = ">>";
    return true;
  }
  return false;
}

string _ltrim(const std::string &s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for (std::string s; iss >> s;)
  {
    args[i] = (char *)malloc(s.length() + 1);
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;
  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line)
{
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos)
  {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&')
  {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line), foreground(true) {}

const char *Command::GetCmd_line()
{
  return cmd_line;
}

bool Command::isForeground(){
  return foreground;
}

void Command::SetForeground(bool fg){
  foreground = fg;
}

/*----- BUILT IN COMMANDS -----*/

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}

ChPromptCommand::ChPromptCommand(const char *cmd_line, std::string &prompt) : BuiltInCommand(cmd_line)
{
  char *args[20];
  _parseCommandLine(cmd_line, args);
  if (!args[1])
    prompt = "smash> ";
  else
    prompt = string(args[1]) + "> ";
}

void ChPromptCommand::execute() {}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  std::cout << "smash pid is " << smash.GetShellPid() << std::endl;
}

PwdCommand::PwdCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void PwdCommand::execute()
{
  char *buffer = NULL;
  string res(getcwd(buffer, 0));
  if (res == "")
  {
    perror("smash error: getcwd failed");
  }
  std::cout << res << std::endl;
}

JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void JobsCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  smash.RemoveFinishedJobs();
  smash.printJobsList();
}

KillCommand::KillCommand(const char *cmd_line, std::shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute()
{
  char *args[20];
  int i = _parseCommandLine(GetCmd_line(), args);
  if (i != 3)
  {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  string signal = string(strdup(const_cast<char *>(args[1])));
  const char* signal_c;
  if (signal.find("-")== 0) {
    signal_c = (signal.substr(signal.find("-")+1)).c_str();
  }
  else{
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  if (atoi(signal_c) == 0 || atoi(args[2]) == 0) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  shared_ptr<JobsList::JobEntry> curr_job = jobs->getJobById(atoi(args[2]));
  if (!curr_job)
  {
    std::cerr << "smash error: kill: job-id " << args[2] << " does not exist" << std::endl;
    return;
  }
  int signum = atoi(signal_c);
  DO_SYS(kill(curr_job->GetPid(), signum), "kill");
  std::cout << "signal number " << signum << " was sent to pid " << curr_job->GetPid() << endl;
  if (signum == SIGCONT)
  {
    curr_job->SetIsStopped(false);
  }
  if (signum == SIGSTOP)
  {
    curr_job->SetIsStopped(true);
  }
}

CdCommand::CdCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void CdCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  char *args[20];
  int i = _parseCommandLine(GetCmd_line(), args);
  if (i == 1)
    return;
  if (smash.GetPrev_pwd() == "" && string(args[1]) == "-")
  {
    std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
    return;
  }
  if (i > 2)
    std::cerr << "smash error: cd: too many arguments" << std::endl;
  if (i == 2)
  {
    char *buffer = NULL;
    std::string temp_prev = smash.GetPrev_pwd();
    string current_dir = getcwd(buffer, 0);
    if (current_dir == "")
    {
      perror("smash error: getcwd failed");
    }
    _removeBackgroundSign(args[1]);
    // change to previous directory
    if (string(args[1]) == "-") 
    {
      smash.SetPrev_Pwd(current_dir);
      DO_SYS(chdir(temp_prev.c_str()), "chdir");
    }
    else
    {
      if (chdir(args[1]) != 0) {//fail: don't change directory
        perror("smash error: chdir failed");
      }
      else{ //success - change dir
        smash.SetPrev_Pwd(current_dir);
      }
    }
  }
}

CatCommand::CatCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void CatCommand::execute()
{
  // SmallShell &smash = SmallShell::getInstance();
  char *args[20];
  int num_args = _parseCommandLine(GetCmd_line(), args);
  char buffer[20];
  if (num_args == 1)
  {
    cout << "smash error: cat: not enough arguments" << endl;
  }
  for (int i = 1; i < num_args; i++)
  {
    if ((strcmp(args[i], ">") == 0) || (strcmp(args[i], ">>") == 0) || (strcmp(args[i], "|") == 0) || (strcmp(args[i], "|&") == 0))
    {
      break;
    }
    string file_name(args[i]);
    int fd_cat = open((_trim(file_name)).c_str(), O_RDWR, 0666); //perror wrap
    if (fd_cat == -1)
    {
      perror("smash error: open failed");
      return;
    }
    int count = 20;
    while (count == 20)
    {
      count = read(fd_cat, buffer, 20);
      if (count == -1)
      {
        perror("smash error: read failed");
        return;
      }
      DO_SYS(write(1, buffer, count), "write");
    }
    DO_SYS(close(fd_cat), "close");
  }
}

QuitCommand::QuitCommand(const char *cmd_line, shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  char *args[20];
  int i = _parseCommandLine(GetCmd_line(), args);
  if (i == 2 && strcmp(args[1], "kill") == 0)
  {
    jobs->killAllJobs();
  }
  else {
    jobs->clearJobsList();
  }
  smash.SetRun(false);
  // exit(0);
}

TimeoutCommand::TimeoutCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void TimeoutCommand::execute()
{
  //add to times list + args[2] should be the external command
  // SmallShell &smash = SmallShell::getInstance();
  char *args[20];
  _parseCommandLine(GetCmd_line(), args);
  string cmd_line(GetCmd_line());
  string args2(args[2]);
  std::size_t pos = cmd_line.find(args2);
  // clean cmd line so only command remains:
  cmd_line = _trim(cmd_line.substr(pos));
  char* cmd_line_c = (char*)(cmd_line.c_str());
  _removeBackgroundSign(cmd_line_c);
  char *bin_bash = strdup(const_cast<char *>("/bin/bash"));
  char *c = strdup(const_cast<char *>("-c"));
  char *exec_args[4] = {bin_bash, c, cmd_line_c, NULL};
  DO_SYS(execvp(exec_args[0], exec_args), "execvp");
  exit(0);
}

void TimeoutCommand::SetPid(pid_t new_pid)
{
  pid = new_pid;
}

pid_t TimeoutCommand::GetPid()
{
  return pid;
}

/*----- FOREGROUND COMMANDS -----*/

ForegroundCommand::ForegroundCommand(const char *cmd_line, shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute()
{
  char *args[20];
  int i = _parseCommandLine(GetCmd_line(), args);
  // maximal job-id to
  shared_ptr<JobsList::JobEntry> cur_job;
  // bring max job to front
  if (i == 1)
  {
    cur_job = jobs->getLastJob();
    if (!cur_job)
    {
      std::cerr << "smash error: fg: jobs list is empty" << std::endl;
      return;
    }
    
  }
  // bring forward specific job-id
  else if (i == 2)
  {
    if (atoi(args[1]) == 0)
    {
      cerr << "smash error: fg: invalid arguments" << endl;
      return;
    }
    else
    {
      cur_job = jobs->getJobById(atoi(args[1]));
      if (!cur_job)
      {
        std::cerr << "smash error: fg: job-id " << args[1] << " does not exist" << std::endl;
        return;
      }
    }
  }
  else
  {
    std::cerr << "smash error: fg: invalid arguments" << std::endl;
    return;
  }
  // remove from jobs list and execute
  jobs->removeJobById(cur_job->GetJobID());
  cout << cur_job->GetCommandLine() << " : " << cur_job->GetPid() << " " << endl;
  SmallShell &smash = SmallShell::getInstance();
  Command* cur_command = cur_job->GetCommand();
  cur_command->SetForeground(true);
  smash.SetCommand(cur_command);
  if (cur_job->isStopped())
  {
    DO_SYS(kill(cur_job->GetPid(), SIGCONT), "kill");
    cur_job->SetIsStopped(false);
  }
  int status;
  DO_SYS(waitpid(cur_job->GetPid(), &status, WUNTRACED), "waitpid");
  if (WIFSTOPPED(status))
  {
    smash.RemoveFinishedJobs();
    jobs->addJobWithId(smash.GetCommand(), cur_job->GetPid(), cur_job->GetJobID(), true);
    return;
  }
}
/*----- BACKGROUND COMMANDS -----*/

BackgroundCommand::BackgroundCommand(const char *cmd_line, shared_ptr<JobsList> jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void BackgroundCommand::execute()
{
  char *args[20];
  int i = _parseCommandLine(GetCmd_line(), args);
  shared_ptr<JobsList::JobEntry> cur_job;
  // bring max job && stopped to front
  if (i == 1)
  {
    cur_job = jobs->getLastStoppedJob();
    if (!cur_job)
    {
      std::cerr << "smash error: bg: there is no stopped jobs to resume" << std::endl;
      return;
    }
  }
  // bring forward specific job-id
  else if (i == 2)
  {
    if (atoi(args[1]) == 0)
    {
      cerr << "smash error: bg: invalid arguments" << endl;
      return;
    }
    else
    {
      cur_job = jobs->getJobById(atoi(args[1]));
      if (!cur_job)
      {
        std::cerr << "smash error: bg: job-id " << args[1] << " does not exist" << std::endl;
        return;
      }
      if (cur_job->isStopped() == false)
      {
        std::cerr << "smash error: bg: job-id " << args[1] << " is already running in the background" << std::endl;
        return;
      }
    }
  }
  else
  {
    std::cerr << "smash error: bg: invalid arguments" << std::endl;
    return;
  }
  // set stopped and execute in background
  cout << cur_job->GetCommandLine() << " : " << cur_job->GetPid() << " " << endl;
  DO_SYS(kill(cur_job->GetPid(), SIGCONT), "kill");
  cur_job->SetIsStopped(false);
}

/*----- EXTERNAL COMMANDS -----*/

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line), pid(-1) {}

void ExternalCommand::execute()
{
  // SmallShell& smash = SmallShell::getInstance();
  char *cmd_line = strdup(const_cast<char *>(GetCmd_line()));
  _removeBackgroundSign(cmd_line);
  char *bin_bash = strdup(const_cast<char *>("/bin/bash"));
  char *c = strdup(const_cast<char *>("-c"));
  char *args[4] = {bin_bash, c, cmd_line, NULL};
  // DO_SYS(execv(args[0], args), "execv");
  DO_SYS(execvp(args[0], args), "execvp");
  exit(0);
}

void ExternalCommand::SetPid(pid_t new_pid)
{
  pid = new_pid;
}

pid_t ExternalCommand::GetPid()
{
  return pid;
}

/*----- JOBS LIST -----*/

JobsList::JobEntry::JobEntry() : job_id(-1), pid(-1), cmd(NULL), is_stopped(false), time(0){};

JobsList::JobEntry::JobEntry(int job_id, pid_t pid, Command *cmd, bool is_stopped, time_t time) : job_id(job_id), pid(pid), cmd(cmd), is_stopped(is_stopped), time(time){};

bool JobsList::JobEntry::isStopped()
{
  return is_stopped;
}

void JobsList::JobEntry::SetIsStopped(bool isStopped)
{
  is_stopped = isStopped;
}

const int &JobsList::JobEntry::GetJobID()
{
  return job_id;
}

pid_t JobsList::JobEntry::GetPid()
{
  return pid;
}

const char *JobsList::JobEntry::GetCommandLine()
{
  return cmd->GetCmd_line();
}

time_t JobsList::JobEntry::GetTime()
{
  return time;
}

Command *JobsList::JobEntry::GetCommand()
{
  return cmd;
}

JobsList::JobsList() : max_job_id(0)
{
  std::vector<std::shared_ptr<JobEntry>> jobs_list;
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped)
{
  Command *new_cmd(cmd);
  shared_ptr<JobEntry> new_job(new JobEntry(++max_job_id, pid, new_cmd, isStopped, time(NULL)));
  jobs_list.push_back(new_job);
  // printJobsList();
}

void JobsList::addJobWithId(Command *cmd, pid_t pid, int job_id, bool isStopped)
{
  Command *new_cmd(cmd);
  shared_ptr<JobEntry> new_job(new JobEntry(job_id, pid, new_cmd, isStopped, time(NULL)));
  if (max_job_id < job_id)
  {
    max_job_id = job_id;
    jobs_list.push_back(new_job);
  }
  else
  {
    for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
    {
      if ((*ir)->GetJobID() > new_job->GetJobID())
      {
        jobs_list.insert(ir, new_job);
        break;
      }
    }
  }
  // printJobsList();
}

void JobsList::printJobsList()
{
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    cout << "[" << (*ir)->GetJobID() << "] " << (*ir)->GetCommandLine() << " : " << (*ir)->GetPid() << " " << difftime(time(NULL), (*ir)->GetTime()) << " secs";
    if ((*ir)->isStopped())
    {
      cout << " (stopped)";
    }
    cout << endl;
  }
}

void JobsList::clearJobsList(){
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    delete (*ir)->GetCommand();
  }
  jobs_list.clear();
}

void JobsList::killAllJobs()
{
  int num_of_jobs = 0;
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    num_of_jobs++;
  }
  cout << "smash: sending SIGKILL signal to " << num_of_jobs << " jobs:" << endl;
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    DO_SYS(kill((*ir)->GetPid(), SIGKILL), "kill");
    cout << (*ir)->GetPid() << ": " << (*ir)->GetCommandLine() << endl;
    delete (*ir)->GetCommand();
  }
  jobs_list.clear();
}

void JobsList::removeFinishedJobs()
{
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    if (waitpid((*ir)->GetPid(), NULL, WNOHANG) > 0)
    {
      int cur_job_id = (*ir)->GetJobID();
      delete (*ir)->GetCommand();
      jobs_list.erase(ir);
      ir--;
      if (cur_job_id == max_job_id)
      {
        if (!jobs_list.empty())
        {
          max_job_id = ((*ir)->GetJobID());
        }
        else
          max_job_id = 0;
      }
    }
  }
}

std::shared_ptr<JobsList::JobEntry> JobsList::getLastJob()
{
  if (jobs_list.empty())
    return nullptr;
  return jobs_list.back();
}

std::shared_ptr<JobsList::JobEntry> JobsList::getLastStoppedJob()
{
  if (jobs_list.empty())
    return nullptr;
  for (auto ir = jobs_list.rbegin(); ir != jobs_list.rend(); ++ir)
  {
    if ((*ir)->isStopped())
    {
      return *ir;
    }
  }
  return nullptr;
}

std::shared_ptr<JobsList::JobEntry> JobsList::getJobById(int jobId)
{
  if(jobId <= 0) {
    return nullptr;
  }
  std::shared_ptr<JobEntry> job_to_remove;
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    if ((*ir)->GetJobID() == jobId)
    {
      std::shared_ptr<JobEntry> job_to_remove = *ir;
      // cout << "get job by id returned: job id " << (*ir)->GetJobID() << " pid: " << (*ir)->GetPid() << endl;
      return job_to_remove;
    }
  }
  return nullptr;
}

std::shared_ptr<JobsList::JobEntry> JobsList::getJobByPid(pid_t pid)
{
  if(pid <= 0) {
    return nullptr;
  }
  std::shared_ptr<JobEntry> job_to_remove;
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    if ((*ir)->GetPid() == pid)
    {
      std::shared_ptr<JobEntry> job_to_remove = *ir;
      // cout << "get job by id returned: job id " << (*ir)->GetJobID() << " pid: " << (*ir)->GetPid() << endl;
      return job_to_remove;
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    if ((*ir)->GetJobID() == jobId)
    {
      int curr_max = (*ir)->GetJobID();
      jobs_list.erase(ir);
      if (jobs_list.empty())
      {
        max_job_id = 0;
      }
      else if (curr_max == max_job_id)
      {
        max_job_id = findMaxJobId();
      }
      return;
    }
  }
}

int JobsList::findMaxJobId()
{
  int max = 0;
  for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir)
  {
    if ((*ir)->GetJobID() > max)
      max = (*ir)->GetJobID();
  }
  return max;
}

JobsList::~JobsList()
{
  // for (auto ir = jobs_list.begin(); ir != jobs_list.end(); ++ir){
  //   *ir = nullptr;
  // }
}

JobsList::JobEntry::~JobEntry(){
  // delete cmd;
}

/*----- TIMES LIST -----*/

TimesList::TimeEntry::TimeEntry() : pid(-1), cmd(NULL), init_time(0), finish_time(0) {};

TimesList::TimeEntry::TimeEntry(pid_t pid, Command* cmd, time_t init_time, time_t finish_time) : pid(pid), cmd(cmd), init_time(init_time), finish_time(finish_time) {};


pid_t TimesList::TimeEntry::GetPid()
{
  return pid;
}

Command *TimesList::TimeEntry::GetCommand() 
{
  return cmd;
}

const char *TimesList::TimeEntry::GetCommandLine()
{
  return cmd->GetCmd_line();
}

time_t TimesList::TimeEntry::GetDuration()
{
  return finish_time - init_time;
}

time_t TimesList::TimeEntry::GetFinishTime()
{
  return finish_time;
}

TimesList::TimesList()
{
  std::vector<std::shared_ptr<TimeEntry>> times_list;
}

void TimesList::addToTimesList(Command* cmd, pid_t pid, time_t duration)
{
  Command *new_cmd(cmd);
  time_t init_time = time(NULL);
  shared_ptr<TimeEntry> new_time(new TimeEntry(pid, new_cmd, init_time, init_time+duration));
  if (times_list.empty()){
    times_list.push_back(new_time); 
    //printTimesList();
    return;
  }
  for (auto ir = times_list.begin(); ir != times_list.end(); ++ir) {
    if((*ir)->GetFinishTime() > init_time + duration) {
      times_list.insert(ir, new_time);
      return;
    }
  }
  times_list.push_back(new_time); 
  //printTimesList();
}


void TimesList::printTimesList()
{
  // cout << "printing times list:" << endl;
  // for (auto ir = times_list.begin(); ir != times_list.end(); ++ir)
  // {
  //   cout << " - " << "Finish Time: " << (*ir)->GetFinishTime() << " Duration: "<< (*ir)->GetDuration() << ", Pid: " << (*ir)->GetPid() << ", " << (*ir)->GetCommandLine() << endl;
  // }
}

void TimesList::killFinishedAlarms()
{
  time_t curr_time = time(NULL);
  if (times_list.empty()){
    return;
  }
  if (times_list.front()->GetFinishTime() > curr_time){
    return;
  }
  SmallShell &smash = SmallShell::getInstance();
  smash.RemoveFinishedJobs();
  JobsList jobs_list = smash.GetJobsListReference();
  if (jobs_list.getJobByPid(times_list.front()->GetPid()) || kill(times_list.front()->GetPid(), 0) == 0) { //the process is still alive
    cout<< "smash: " << times_list.front()->GetCommandLine() << " timed out!" <<endl;
    DO_SYS(kill(times_list.front()->GetPid(), SIGKILL), "kill");
  }
  time_t new_alarm_time = difftime(times_list.front()->GetFinishTime(), curr_time);
  while (new_alarm_time <= 0) {
    times_list.erase(times_list.begin());
    if (times_list.empty()){
      return;
    }
    new_alarm_time = difftime(times_list.front()->GetFinishTime(), curr_time);
  }
  alarm(new_alarm_time);
}

time_t TimesList::GetClosestAlarm() {
  if (times_list.empty()){
    return 0;
  }
  return times_list.front()->GetDuration();
}


/*----- SMASH IMPLEMENTATION -----*/

SmallShell::SmallShell() : run(true), prompt("smash> "), prev_pwd(""), jobs_list(), times_list(), current_cmd(nullptr), shell_pid(getpid()) {}

SmallShell::~SmallShell()
{
  // delete current_cmd;
}

Command *SmallShell::GetCommand()
{
  return current_cmd;
}

void SmallShell::SetCommand(Command *cmd_new)
{
  current_cmd = cmd_new;
}

shared_ptr<JobsList> SmallShell::GetJobsList()
{
  shared_ptr<JobsList> jobs_list_ptr(&jobs_list);
  return jobs_list_ptr;
}

JobsList& SmallShell::GetJobsListReference()
{
  return jobs_list;
}

TimesList& SmallShell::GetTimesListReference()
{
  return times_list;
}

std::shared_ptr<TimesList> SmallShell::GetTimesList() {
  shared_ptr<TimesList> times_list_ptr(&times_list);
  return times_list_ptr;
}


bool SmallShell::GetRun()
{
  return run;
}

void SmallShell::SetRun(bool new_run)
{
  run = new_run;
}

pid_t SmallShell::GetShellPid(){
  return shell_pid;
}

std::string &SmallShell::GetPrompt()
{
  return prompt;
}

std::string &SmallShell::GetPrev_pwd()
{
  return prev_pwd;
}

void SmallShell::SetPrompt(std::string new_prompt)
{
  prompt = new_prompt;
}

void SmallShell::SetPrev_Pwd(std::string new_prev_pwd)
{
  prev_pwd = new_prev_pwd;
}

/*
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line)
{
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n>|&"));
  if (firstWord.compare("chprompt") == 0)
  {
    return new ChPromptCommand(cmd_line, prompt);
  }
  else if (firstWord.compare("showpid") == 0)
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0)
  {
    return new PwdCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0)
  {
    return new CdCommand(cmd_line);
  }
  else if (firstWord.compare("fg") == 0)
  {
    SmallShell &smash = SmallShell::getInstance();
    return new ForegroundCommand(cmd_line, smash.GetJobsList());
  }
  else if (firstWord.compare("bg") == 0)
  {
    SmallShell &smash = SmallShell::getInstance();
    return new BackgroundCommand(cmd_line, smash.GetJobsList());
  }
  else if (firstWord.compare("jobs") == 0)
  {
    return new JobsCommand(cmd_line);
  }
  else if (firstWord.compare("kill") == 0)
  {
    SmallShell &smash = SmallShell::getInstance();
    return new KillCommand(cmd_line, smash.GetJobsList());
  }
  else if (firstWord.compare("cat") == 0)
  {
    return new CatCommand(cmd_line);
  }
  else if (firstWord.compare("quit") == 0)
  {
    SmallShell &smash = SmallShell::getInstance();
    return new QuitCommand(cmd_line, smash.GetJobsList());
  }
  else if (firstWord.compare("timeout") == 0)
  {
    return new TimeoutCommand(cmd_line);
  }
  else
  {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

// TODO: DEBUG!!!!!!!!
void SmallShell::executePipeCommand(const char *cmd_line, string &type)
{
  int fd[2];
  DO_SYS(pipe(fd), "pipe");
  string cmd_line_new(cmd_line);
  string cmd_line1 = cmd_line_new.substr(0, cmd_line_new.find_first_of(type));
  string cmd_line2 = cmd_line_new.substr(cmd_line_new.find_first_of(type) + type.length());
  Command *cmd_1 = CreateCommand(cmd_line1.c_str());
  Command *cmd_2 = CreateCommand(cmd_line2.c_str());
  int res = fork();
  if (res == -1)
  {
    perror("smash error: open failed");
  }
  if (res == 0)
  { 
    // first child
    if (type.compare("|&") == 0)
    {
      DO_SYS(dup2(fd[1], 2), "dup2");
    }
    else
    {
      DO_SYS(dup2(fd[1], 1), "dup2");
    }
    DO_SYS(close(fd[0]), "close");
    DO_SYS(close(fd[1]), "close");
    DO_SYS(setpgrp(), "setpgrp");
    cmd_1->execute();
    exit(0);
  }
  // second child
  int res2 = fork();
  if (res2 == 0)
  {
    DO_SYS(dup2(fd[0], 0), "dup2");
    DO_SYS(close(fd[0]), "close");
    DO_SYS(close(fd[1]), "close");
    DO_SYS(setpgrp(), "setpgrp");
    cmd_2->execute();
    exit(0);
  }
  if (res2 == -1)
  {
    perror("smash error: open failed");
  }
  DO_SYS(close(fd[0]), "close");
  DO_SYS(close(fd[1]), "close");
  int status;
  DO_SYS(waitpid(res, &status, 0), "waitpid");
  DO_SYS(waitpid(res2, &status, 0), "waitpid");
}

// TODO: DEBUG!!!!!!!!
int SmallShell::prepareRedirectionCommand(char *cmd_line, string &type)
{
  string cmd_line_new(cmd_line);
  strcpy(cmd_line, cmd_line_new.substr(0, cmd_line_new.find_first_of(type)).c_str());
  string file_name = _trim(cmd_line_new.substr(cmd_line_new.find_first_of(type) + type.length()));
  int fd_stdout = dup(1);
  if (fd_stdout == -1)
  {
    perror("smash error: dup failed");
    return -1;
  }
  int fd;
  if (close(1) == -1)
  {
    perror("smash error: close failed");
    close(fd_stdout);
    return -1;
  }
  if (type.compare(">>") == 0)
  {
    fd = open(file_name.c_str(), O_CREAT | O_RDWR | O_APPEND, 0666); //perror wrap
  }
  else
  {
    fd = open(file_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
  }
  if (fd == -1)
  {
    perror("smash error: open failed");
    dup2(fd_stdout, 1);
    return -1;
  }
  return fd_stdout;
}

void SmallShell::executeCommand(const char *cmd_line)
{
  char *cmd_line_new = strdup(const_cast<char *>(cmd_line));
  bool is_background = _isBackgroundComamnd(cmd_line);
  string redirection_type;
  string pipe_type;
  bool redirection = _isRedirectionCommand(cmd_line, redirection_type);
  bool pipe = _isPipeCommand(cmd_line, pipe_type);
  if (pipe)
  {
    executePipeCommand(cmd_line, pipe_type);
    return;
  }
  int fd_stdout = 0;
  if (redirection)
  {
    fd_stdout = prepareRedirectionCommand(cmd_line_new, redirection_type); // careful with this one
    if (fd_stdout == -1) {
      return;
    }
  }
  Command *cmd = CreateCommand(cmd_line_new);
  //External Command:
  if (typeid(*cmd) == typeid(ExternalCommand) || typeid(*cmd) == typeid(TimeoutCommand))
  { 
    if (is_background){
      cmd->SetForeground(false);
    }
    SetCommand(cmd);
    int duration = -1;
    pid_t pid = fork();
    if (pid == -1)
    {
      perror("smash error: fork failed");
    }
    // child
    if (pid == 0)
    {
      DO_SYS(setpgrp(), "setpgrp");
      cmd->execute();
      exit(0);
    }
    // parent
    else
    {
      cmd->SetPid(pid);
      if (typeid(*cmd) == typeid(TimeoutCommand)) {
        char *args[20];
        _parseCommandLine(cmd->GetCmd_line(), args);
        duration = atoi(args[1]);
        if (times_list.GetClosestAlarm() == 0 || times_list.GetClosestAlarm() > duration){
          // cout << "set alarm for duration:" << duration << endl; //DEBUGPRINT
          alarm(duration);
        }
        times_list.addToTimesList(cmd, pid, duration);
      }
      SmallShell &smash = SmallShell::getInstance();
      if (is_background)
      {
        smash.RemoveFinishedJobs();
        jobs_list.addJob(cmd, pid, false);
      }
      else
      {
        int status;
        DO_SYS(waitpid(pid, &status, WUNTRACED), "waitpid");
        if (WIFSTOPPED(status))
        {
          smash.RemoveFinishedJobs();
          jobs_list.addJob(cmd, pid, true);
          return;
        }
      }
    }
  }
  // Built in Commands:
  else
  {
    cmd->execute();
    // delete cmd;
  }
  if (redirection)
  {
    DO_SYS(dup2(fd_stdout, 1), "dup2");
    DO_SYS(close(fd_stdout), "close");
  }
}

void SmallShell::RemoveFinishedJobs()
{
  jobs_list.removeFinishedJobs();
}

void SmallShell::printJobsList()
{
  // cout << "shell printing jobs list:" << endl;
  jobs_list.printJobsList();
}
