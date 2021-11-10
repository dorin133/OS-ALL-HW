#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <memory>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

using namespace std;

bool _isBackgroundComamnd(const char *cmd_line);

class Command {
  const char* cmd_line;
  bool foreground;
 public:
  Command(const char* cmd_line);
  // Command(const char* cmd_line, bool fg);
  virtual ~Command() {free((char*)cmd_line);}
  virtual void execute() = 0;
  const char* GetCmd_line();
  bool isForeground();
  void SetForeground(bool fg);
  virtual void SetPid(pid_t new_pid) {};
  virtual pid_t GetPid() {return -1;};
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
  pid_t pid;
 public:
  ExternalCommand(const char* cmd_line);
  ExternalCommand(const char* cmd_line, bool fg);
  virtual ~ExternalCommand() {}
  void execute() override;
  void SetPid(pid_t new_pid) override;
  pid_t GetPid() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 
 public:
 //should have a default ctor
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members public:
  ChangeDirCommand(const char* cmd_line, char** plastPwd);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ChPromptCommand : public BuiltInCommand {
 public:
  ChPromptCommand(const char* cmd_line, std::string& prompt);
  virtual ~ChPromptCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class PwdCommand : public BuiltInCommand {
 public:
  PwdCommand(const char* cmd_line);
  virtual ~PwdCommand() {}
  void execute() override;
};

class CdCommand : public BuiltInCommand {
 public:
  CdCommand(const char* cmd_line);
  virtual ~CdCommand() {}
  void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
  shared_ptr<JobsList> jobs;
public:
  QuitCommand(const char* cmd_line, shared_ptr<JobsList> jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList {
  public:
  class JobEntry {
   int job_id;
   pid_t pid;
   Command* cmd;
   bool is_stopped;
   time_t time;
   public:
   JobEntry();
   JobEntry(int job_id, pid_t pid, Command* cmd, bool is_stopped, time_t init_time);
   ~JobEntry();
   bool isStopped();
   void SetIsStopped(bool is_stopped);
   const int& GetJobID();
   pid_t GetPid();
   const char* GetCommandLine();
   time_t GetTime();
   Command* GetCommand();
  };
  private:
  std::vector<std::shared_ptr<JobEntry>> jobs_list;
  int max_job_id;
 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, pid_t pid, bool isStopped);
  void addJobWithId(Command* cmd, pid_t pid, int job_id, bool isStopped);
  void printJobsList();
  void killAllJobs();
  void clearJobsList();
  void removeFinishedJobs();
  std::shared_ptr<JobEntry> getJobById(int jobId);
  std::shared_ptr<JobEntry> getJobByPid(pid_t pid);
  void removeJobById(int jobId);
  std::shared_ptr<JobEntry> getLastJob();
  std::shared_ptr<JobEntry> getLastStoppedJob();
  JobEntry *getLastStoppedJob(int *jobId);
  int findMaxJobId();
};

class JobsCommand : public BuiltInCommand {
 public:
  JobsCommand(const char* cmd_line);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
  std::shared_ptr<JobsList> jobs;
 public:
  KillCommand(const char* cmd_line, std::shared_ptr<JobsList> jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
  std::shared_ptr<JobsList> jobs;
 public:
  ForegroundCommand(const char* cmd_line, std::shared_ptr<JobsList> jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 std::shared_ptr<JobsList> jobs;
 public:
  BackgroundCommand(const char* cmd_line, std::shared_ptr<JobsList> jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class CatCommand : public BuiltInCommand {
 public:
  CatCommand(const char* cmd_line);
  virtual ~CatCommand() {}
  void execute() override;
};

/* ---- TIMED COMMANDS ---- */

class TimesList {
  public:
  class TimeEntry {
   pid_t pid;
   Command* cmd;
   time_t init_time;
   time_t finish_time; //finish_time = (time stamp at start) + (duration)
   public:
   TimeEntry();
   TimeEntry(pid_t pid, Command* cmd, time_t init_time, time_t finish_time);
   ~TimeEntry() { };
   pid_t GetPid();
   const char* GetCommandLine();
   time_t GetFinishTime();
   time_t GetDuration();
   Command* GetCommand();
  };
  private:
 public:
  std::vector<std::shared_ptr<TimeEntry>> times_list;
  TimesList();
  ~TimesList() {};
  void addToTimesList(Command* cmd, pid_t pid, time_t duration);
  time_t GetClosestAlarm();
  void printTimesList();
  void killFinishedAlarms();
};

class TimeoutCommand : public BuiltInCommand {
  pid_t pid;
 public:
  TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
  void SetPid(pid_t new_pid) override;
  pid_t GetPid() override;
};



class SmallShell {
 private:
  SmallShell();
  bool run;
  std::string prompt;
  std::string prev_pwd;
  JobsList jobs_list;
  TimesList times_list;
  Command* current_cmd;
  pid_t shell_pid;
 public:
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  void executePipeCommand(const char *cmd_line, string& type);
  int prepareRedirectionCommand(char *cmd_line, string& type);
  bool GetRun();
  pid_t GetShellPid();
  std::string& GetPrompt();
  std::string& GetPrev_pwd();
  JobsList& GetJobsListReference();
  std::shared_ptr<JobsList> GetJobsList();
  std::shared_ptr<TimesList> GetTimesList();
  TimesList& GetTimesListReference();
  Command* GetCommand();
  void SetCommand(Command* cmd_new);
  void RemoveFinishedJobs();
  void SetPrompt(std::string new_prompt);
  void SetPrev_Pwd(std::string new_prev_pwd);
  void SetRun(bool new_run);
  void printJobsList();
};

#endif //SMASH_COMMAND_H_