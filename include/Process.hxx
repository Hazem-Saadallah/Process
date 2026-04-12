#pragma once

#include <Impl/Process.hxx>
#include <Impl/Datatype.hxx>

namespace Process {
  typedef Impl::Process Process;
  typedef Impl::Process::Status Status;
  typedef Impl::Process::FileSaveStatus FileSaveStatus;
  using namespace Impl::Datatype;
}
