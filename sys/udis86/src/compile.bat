set WXPBASE=C:\WinDDK\6001.18000
cd %0\..
call ddkbuild.bat -WXP chk .
@copy objchk_wlh_x86\i386\*.lib ..\
pause