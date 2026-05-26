@echo off
setlocal
pushd "%~dp0.."
set "REPO_ROOT=%CD%"

where py >nul 2>nul
if %errorlevel%==0 (
  start "" py -3 "%REPO_ROOT%\tools\skill_feedback\open_feedback_form.py"
  goto done
)

where pythonw >nul 2>nul
if %errorlevel%==0 (
  start "" pythonw "%REPO_ROOT%\tools\skill_feedback\open_feedback_form.py"
  goto done
)

python "%REPO_ROOT%\tools\skill_feedback\open_feedback_form.py"

:done
popd
