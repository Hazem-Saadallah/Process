#include <cerrno>
#include <functional>
#include <string>
#include <fstream>
#include <Impl/Datatype.hxx>
#include <Impl/Process.hxx>
#include <vector>

Process::Process(Datatype::ArgList args_list) : m_ArgList(args_list) { }
Process::~Process() { if(m_WorkerThread.joinable()) m_WorkerThread.join(); }

#if defined (__linux__)
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void Process::exec_linux(bool sync) {
  std::int32_t stdout_pipe[2], stderr_pipe[2], exec_pipe[2];
  if(pipe(stdout_pipe) == -1) {
    m_Status = Process::Status::PIPE_FAILURE;
    return;
  };
  if(pipe(stderr_pipe) == -1) {
    close(stdout_pipe[0]), close(stdout_pipe[1]);
    m_Status = Process::Status::PIPE_FAILURE;
    return;
  }
  if(pipe2(exec_pipe, O_CLOEXEC) == -1) {
    close(stdout_pipe[0]), close(stdout_pipe[1]), close(stderr_pipe[0]), close(stderr_pipe[1]);
    m_Status = Process::Status::PIPE_FAILURE;
    return;
  }

  pid_t pid = fork();
  if(pid < 0) {
    m_Status = Process::Status::FORK_FAILURE;
    return;
  } else if(pid == 0) {
    close(stdout_pipe[0]), close(stderr_pipe[0]), close(exec_pipe[0]);
    dup2(stdout_pipe[1], STDOUT_FILENO), dup2(stderr_pipe[1], STDERR_FILENO);
    close(stdout_pipe[1]), close(stderr_pipe[1]);

    std::vector<char*> args;
    args.reserve(m_ArgList.size()+1);
    for(std::uint16_t i{0}; i < m_ArgList.size(); ++i) args.push_back(m_ArgList[i].data());
    args.push_back(nullptr);

    execvp(args.at(0), args.data());
    std::int32_t err = errno;
    write(exec_pipe[1], &err, sizeof(err));
    _exit(127);
  } else {
    m_PID = pid;
    close(stdout_pipe[1]), close(stderr_pipe[1]), close(exec_pipe[1]);
    std::int32_t err_code;
    ssize_t bytes_read = read(exec_pipe[0], &err_code, sizeof(err_code));
    close(exec_pipe[0]);
    if(bytes_read == sizeof(err_code)) {
      m_Status = Process::Status::EXECUTION_FAILURE;
      std::int32_t status;
      waitpid(pid, &status, 0);
      m_ExitCode = WEXITSTATUS(status);
      close(stdout_pipe[0]), close(stderr_pipe[0]);
      return;
    }
    m_Status = Process::Status::RUNNING;
    std::function<void()> worker_task = [this, pid, out_fd=stdout_pipe[0], err_fd=stderr_pipe[0]](){
      pollfd pfds[2];
      pfds[0].fd = out_fd, pfds[0].events = POLLIN;
      pfds[1].fd = err_fd, pfds[1].events = POLLIN;

      std::int32_t active_fds{2};
      std::string buffer(4096, '\0');

      while (active_fds > 0) {
        std::int32_t poll_result = poll(pfds, 2, -1);
        if(poll_result == -1) break;
        for(std::uint8_t i{0}; i < 2; ++i) {
          if(pfds[i].revents & (POLLIN|POLLHUP|POLLERR)) {
            ssize_t bytes_read = read(pfds[i].fd, buffer.data(), buffer.size());
            if(bytes_read > 0) {
              std::lock_guard<std::mutex> lock(m_Mutex);
              i==0? m_Stdout.append(buffer.data(), bytes_read): m_Stderr.append(buffer.data(), bytes_read);
            } else {
              pfds[i].fd = -1;
              --active_fds;
            }
          }
        }
      }
      close(out_fd), close(err_fd);

      std::int32_t status;
      waitpid(pid, &status, 0);

      std::lock_guard<std::mutex> lock(m_Mutex);
      if (WIFEXITED(status)) {
        m_Status = Process::Status::FINISHED;
        m_ExitCode = WEXITSTATUS(status);
      } else if(WIFSIGNALED(status)) {
        m_Status = Process::Status::SIGNALED;
        m_ExitCode = 128+WTERMSIG(status);
      }
    };
    if(sync) worker_task();
    else {
      if(m_WorkerThread.joinable()) m_WorkerThread.join();
      m_WorkerThread = std::thread(worker_task);
    }
  }
}

#elif defined (__WIN32)
#error "Unsupported operating system"
void Process::exec_macos(bool sync) {}

#elif defined(__APPLE__) && defined(__MACH__)
#error "Unsupported operating system"
void Process::exec_windows(bool sync) {}

#else
#error "Unsupported operating system"

#endif

void Process::run(bool sync) {
#if defined (__linux__)
  exec_linux(sync);

#elif defined (__WIN32)
#error "Unsupported operating system"
  exec_windows(sync);

#elif defined(__APPLE__) && defined(__MACH__)
#error "Unsupported operating system"
  exec_macos(sync);

#else
#error "Unsupported operating system"

#endif
}

void Process::exec(bool sync) { run(sync); }


void Process::set_args(Datatype::ArgList args_list) { m_ArgList = args_list; }
Datatype::ArgList Process::get_args() const { return m_ArgList; }
bool Process::is_executable() const { return !m_ArgList.empty(); }

std::string Process::get_stdout() const {
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Stdout;
}
std::string Process::get_stderr() const {
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Stderr;
}

inline Process::FileSaveStatus save_to_file(const std::string& file_path, const std::string& data) {
  std::ofstream out_file(file_path, std::ios::out | std::ios::trunc);
  if(!out_file.is_open()) return Process::FileSaveStatus::OPEN_FAILED;

  out_file.write(data.data(), data.size());
  if(out_file.fail() || out_file.bad()) {
    out_file.close();
    return Process::FileSaveStatus::WRITE_FAILED;
  }

  out_file.close();
  if(out_file.fail()) return Process::FileSaveStatus::CLOSE_FAILED;

  return Process::FileSaveStatus::SUCCESS;
}

Process::FileSaveStatus Process::save_stdout(const std::string& file_path) const { return save_to_file(file_path, m_Stdout); }
Process::FileSaveStatus Process::save_stderr(const std::string& file_path) const { return save_to_file(file_path, m_Stderr); }

Process::Status Process::get_status() const {
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status;
}

std::int16_t Process::get_exit_code() const {
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_ExitCode;
}

pid_t Process::get_process_id() const { return m_PID; }
