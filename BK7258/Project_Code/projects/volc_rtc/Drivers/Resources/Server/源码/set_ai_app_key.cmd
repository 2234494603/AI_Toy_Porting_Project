@echo off
setlocal
title Xiaoqin Desktop Service Setup
pushd "%~dp0" >nul
if errorlevel 1 (
  echo Cannot open the service directory.
  pause
  exit /b 1
)
chcp 65001 >nul
where pyw >nul 2>nul
if not errorlevel 1 (
  start "" pyw -3 ".\start_gui.pyw" --setup
) else (
  where pythonw >nul 2>nul
  if errorlevel 1 (
    echo Python 3 was not found. Please install Python with Tkinter support.
    pause
    popd
    exit /b 1
  )
  start "" pythonw ".\start_gui.pyw" --setup
)
popd
exit /b 0
