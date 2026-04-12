#pragma once

#include <mutex>
#include <thread>
#include <cstdint>
#include <Impl/Datatype.hxx>

#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/types.h>
#endif

namespace Impl {
class Process {
public:
  enum class Status : std::uint8_t {
    UNINITIALIZED=0,
    RUNNING,
    FINISHED,
    SIGNALED,
    EXIT_UNKNOWN,
    PIPE_FAILURE,
    FORK_FAILURE,
    EXECUTION_FAILURE,
  };

  enum class FileSaveStatus : std::uint8_t {
    SUCCESS=0,
    OPEN_FAILED,
    WRITE_FAILED,
    CLOSE_FAILED
  };

private:
  Datatype::ArgList m_ArgList={};
  std::string m_Stdout{""};
  std::string m_Stderr{""};
  Impl::Datatype::ExitCode m_ExitCode{0};
  Status m_Status{Process::Status::UNINITIALIZED};
  pid_t m_PID{0};
  mutable std::mutex m_Mutex;
  std::thread m_WorkerThread;

public:
  Process() = default;
  Process(Datatype::ArgList args_list);
  ~Process();
  Process(Process &&) noexcept = delete;
  Process &operator=(Process &&) noexcept = delete;
  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;

private:
#if defined (__linux__)
  void exec_linux(bool sync);

#elif defined (__WIN32)
#error "Unsupported operating system"

#elif defined(__APPLE__) && defined(__MACH__)
#error "Unsupported operating system"
  void exec_windows(bool sync);

#else
#error "Unsupported operating system"
  void exec_macos(bool sync);
#endif

public:
  void run(bool sync=true); /* NOTE: Same as exec() */
  void exec(bool sync=true); /* NOTE: Same as run() */

  void set_args(Datatype::ArgList args_list);
  Datatype::ArgList get_args() const;
  bool is_executable() const;

  Impl::Datatype::Stdout get_stdout() const;
  Impl::Datatype::Stderr get_stderr() const;

  FileSaveStatus save_stdout(const std::string& file_path) const;
  FileSaveStatus save_stderr(const std::string& file_path) const;

  Status get_status() const;
  Impl::Datatype::ExitCode get_exit_code() const;
  pid_t get_process_id() const;
};
}
