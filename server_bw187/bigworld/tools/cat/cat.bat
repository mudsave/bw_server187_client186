@echo Please examine settings below and modify if necessary
cd \mf\bigworld\tools\cat
set MF_ROOT=/mf
set BW_RES_PATH=/mf/fantasydemo/res;/mf/bigworld/res
@if exist main.py (python main.py) else (python main.pyc)
pause
