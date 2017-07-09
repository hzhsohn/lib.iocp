rem "清除SDK部分"

del "sdk\*.ncb"
del "sdk\*.suo" /a
del "sdk\*.sdf" /a
del "sdk\*.user"
rd "sdk\debug" /s/q
rd "sdk\Release" /s/q
rd "sdk\ipch" /s/q

rem "清除例子"
del "test_serv\*.ncb"
del "test_serv\*.aps"
del "test_serv\*.suo" /a
del "test_serv\*.sdf" /a
del "test_serv\*.user"
rd "test_serv\debug" /s/q
rd "test_serv\Release" /s/q
rd "test_serv\ipch" /s/q

del "test_client\*.ncb"
del "test_client\*.aps"
del "test_client\*.suo" /a
del "test_client\*.sdf" /a
del "test_client\*.user"
rd "test_client\debug" /s/q
rd "test_client\Release" /s/q
rd "test_client\ipch" /s/q

rem "清理已编译项"

del "bin\debug\*.exp"
del "bin\debug\*.ilk"
del "bin\debug\*.pdb"

del "bin\release\*.exp"
del "bin\release\*.map"
del "bin\release\*.pdb"
del "bin\release\*.ilk"

rem cd ZhCTcp
rem start clean.bat