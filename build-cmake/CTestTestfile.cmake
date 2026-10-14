# CMake generated Testfile for 
# Source directory: /home/user/humtrace
# Build directory: /home/user/humtrace/build-cmake
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_compare]=] "/home/user/humtrace/build-cmake/bin/test_compare")
set_tests_properties([=[test_compare]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_pipeline]=] "/home/user/humtrace/build-cmake/bin/test_pipeline")
set_tests_properties([=[test_pipeline]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_report]=] "/home/user/humtrace/build-cmake/bin/test_report")
set_tests_properties([=[test_report]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_sha256]=] "/home/user/humtrace/build-cmake/bin/test_sha256")
set_tests_properties([=[test_sha256]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_spectrum]=] "/home/user/humtrace/build-cmake/bin/test_spectrum")
set_tests_properties([=[test_spectrum]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_tracker]=] "/home/user/humtrace/build-cmake/bin/test_tracker")
set_tests_properties([=[test_tracker]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_view]=] "/home/user/humtrace/build-cmake/bin/test_view")
set_tests_properties([=[test_view]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_wav]=] "/home/user/humtrace/build-cmake/bin/test_wav")
set_tests_properties([=[test_wav]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[test_wav_fuzz]=] "/home/user/humtrace/build-cmake/bin/test_wav_fuzz")
set_tests_properties([=[test_wav_fuzz]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;35;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[cli_integration]=] "/usr/bin/python3" "/home/user/humtrace/tests/test_cli.py" "/home/user/humtrace/build-cmake/bin/humtrace")
set_tests_properties([=[cli_integration]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;39;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[app_test_gui]=] "/home/user/humtrace/build-cmake/bin/app_test_gui")
set_tests_properties([=[app_test_gui]=] PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;66;add_test;/home/user/humtrace/CMakeLists.txt;0;")
add_test([=[app_test_presentation]=] "/home/user/humtrace/build-cmake/bin/app_test_presentation")
set_tests_properties([=[app_test_presentation]=] PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/user/humtrace/CMakeLists.txt;66;add_test;/home/user/humtrace/CMakeLists.txt;0;")
