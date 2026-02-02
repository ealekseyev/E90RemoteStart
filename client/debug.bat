@echo off
if "%1"=="" (
    py -3.12 "%~dp0can_reader.py" COM3 --blacklist "%~dp0blacklist_new.json"
) else (
    py -3.12 "%~dp0can_reader.py" COM3 --log %1.log --blacklist "%~dp0blacklist_new.json"
)