@echo on

 for /R %%x in (*.bat) do ( 
 if not %%x == %~0 (cd %%~dpx & call %%x & cd %~dp0)
)
pause