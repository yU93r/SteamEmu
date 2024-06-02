@echo off

setlocal

set /a exit=0

set "file=%~1"
if not defined file (
  set /a exit=1
  goto :end_script
)

pushd "%~dp0"
set "OPENSSL_CONF=%cd%\openssl.cnf"

set "filename=%random%"
for %%A in ("%file%") do (
  set "filename=%random%-%%~nxA"
)

:re_pvt
call :gen_rnd rr
set "pvt_file=%cd%\prvt-%rr%-%filename%.pem"
:: parallel build can generate same rand number
if exist "%pvt_file%" (
  goto :re_pvt
)

:re_cer
call :gen_rnd rr
set "cer_file=%cd%\cert-%rr%-%filename%.pem"
:: parallel build can generate same rand number
if exist "%cer_file%" (
  goto :re_cer
)

:re_pfx
call :gen_rnd rr
set "pfx_file=%cd%\cfx-%rr%-%filename%.pfx"
:: parallel build can generate same rand number
if exist "%pfx_file%" (
  goto :re_pfx
)

set "openssl_exe=%cd%\openssl.exe"
set "signtool_exe=%cd%\signtool.exe"

popd

call "%openssl_exe%" req -newkey rsa:2048 -nodes -keyout "%pvt_file%" -x509 -days 5525 -out "%cer_file%" ^
  -subj "/O=GSE/CN=GSE" ^
  -addext "extendedKeyUsage=codeSigning" ^
  -addext "basicConstraints=critical,CA:true" ^
  -addext "subjectAltName=email:GSE,DNS:GSE,DNS:GSE" ^
  -addext "keyUsage=digitalSignature,keyEncipherment" ^
  -addext "authorityKeyIdentifier=keyid,issuer:always" ^
  -addext "crlDistributionPoints=URI:GSE" ^
  -addext "subjectKeyIdentifier=hash" ^
  -addext "issuerAltName=issuer:copy" ^
  -addext "nsComment=GSE" ^
  -extensions v3_req
set /a exit+=%errorlevel%
if %exit% neq 0 (
  goto :end_script
)

call "%openssl_exe%" pkcs12 -export -out "%pfx_file%" -inkey "%pvt_file%" -in "%cer_file%" -passout pass:
set /a exit+=%errorlevel%

del /f /q "%cer_file%"
del /f /q "%pvt_file%"

if %exit% neq 0 (
  goto :end_script
)

call "%signtool_exe%" sign /d "GSE" /fd sha256 /f "%pfx_file%" /p "" "%~1"
set /a exit+=%errorlevel%
if %exit% neq 0 (
  goto :end_script
)

del /f /q "%pfx_file%"

:end_script
endlocal
exit /b %exit%


:: when every project is built in parallel '/MP' with Visual Studio,
:: the regular random variable might be the same, causing racing
:: this will waste some time and hopefully generate a different number
:: 1: (ref) out random number
:gen_rnd
  setlocal enabledelayedexpansion 
  for /l %%A in (1, 1, 10) do (
    set "_r=!random!"
  )
endlocal & (
  set "%~1=%random%"
  exit /b
)
