@echo off
git ls-files --eol | findstr "i/crlf i/mixed"
if %errorlevel%==1 (
  echo EOL check: Ok
  exit /b 0
)
echo EOL check: Error
exit /b 1
