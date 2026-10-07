@rem Generates the Doxygen documentation through the Ninja build that build_windows.cmd configures.
@call "%~dp0build_windows.cmd" debug --no-tests --docs
