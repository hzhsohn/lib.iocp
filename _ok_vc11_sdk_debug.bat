@set path="C:\Program Files\Microsoft Visual Studio 11.0\Common7\IDE\";%path%
@set path="D:\Microsoft Visual Studio 11.0\Common7\IDE\";%path%
@cls

devenv "sdk\IocpNet.vcxproj" /build debug
devenv "test\TestGCSRTNetServer.vcxproj" /build debug