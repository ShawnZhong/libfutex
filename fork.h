#pragma once

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdexcept>
#include <vector>

template <typename Fn>
static void fork_and_wait(size_t nproc, const Fn& fn) {
  std::vector<pid_t> pids;
  pids.reserve(nproc);
  for (int i = 0; i < nproc; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      fn(i);
      exit(0);
    } else if (pid > 0) {
      pids.emplace_back(pid);
      SPDLOG_DEBUG("Forked child {} (pid {})", i, pid);
    } else {
      throw std::runtime_error("fork failed");
    }
  }

  for (int i = 0; i < nproc; i++) {
    int status;
    if (waitpid(pids[i], &status, 0) == -1) {
      throw std::runtime_error("waitpid failed");
    }

    if (WIFEXITED(status)) {
      SPDLOG_DEBUG("Child {} (pid {}) exited normally with status {}", i,
                  pids[i], WEXITSTATUS(status));
      continue;
    }

    if (WIFSIGNALED(status)) {
      SPDLOG_WARN(
          "Child {} (pid {}) killed by signal \"{}\". Killing other "
          "children...",
          i, pids[i], strsignal(WTERMSIG(status)));
      for (int j = i; j < nproc; j++) {
        kill(pids[j], SIGTERM);
      }
      exit(1);
    }

    SPDLOG_ERROR("Child {} (pid {}) exited abnormally with status {}", i,
                 pids[i], status);
  }
}

template <typename Fn>
static void fork_and_wait(const Fn& fn) {
  fork_and_wait(1, [&](int) { fn(); });
}
