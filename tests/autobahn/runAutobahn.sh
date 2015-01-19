#!/bin/bash

function no_wstest() {
  echo '*** Error: Failed to find "wstest" of the Autobahn Testsuite.'
  echo '    Please make sure "wstest" is in your $PATH.'
  echo '    To obtain a copy of it, see http://autobahn.ws/testsuite/installation.html'
  echo '    Disclaimer: The NuriaProject is not affiliated with Autobahn in any way.'
  exit 1
}

# Check for wstest
command -v wstest > /dev/null 2>&1 || no_wstest

# Check that 'autobahn' has been built
Server="$1"

if [ ! -f "$Server" ]; then
  if command -v autobahn > /dev/null 2>&1; then
    Server="autobahn"
  elif [ -f "./autobahn" ]; then
    Server="./autobahn"
  else
    echo '*** Error: Failed to find the "autobahn" binary.'
    echo '    Make sure that either you pass its path to this script,'
    echo '    have it in your $PATH or in your current working directory.'
    exit 2
  fi
fi

# Start the autobahn test server
echo "*** Starting autobahn test server at ${Server}"
$Server > /dev/null 2>&1 &
ServerPid="$!"
sleep 1

# Start the test
wstest -m fuzzingclient

# Stop server
kill $ServerPid

# 
echo
echo '*** If wstest finished successfully, you can now find the generated report at:'
echo "    file://${PWD}/reports/servers/index.html"
