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

set "pvt_file=%cd%\prvt.pem"
set "cer_file=%cd%\cert.pem"
set "pfx_file=%cd%\cfx.pfx"

set "openssl_exe=%cd%\openssl.exe"
set "signtool_exe=%cd%\signtool.exe"

popd

call "%openssl_exe%" req -newkey rsa:2048 -nodes -keyout "%pvt_file%" -x509 -days 5525 -out "%cer_file%" ^
  -subj "/O=Goldberg Emu/CN=Goldberg Emu" ^
  -addext "extendedKeyUsage=codeSigning" ^
  -addext "basicConstraints=critical,CA:true" ^
  -addext "subjectAltName=email:Goldberg Emu,DNS:Goldberg Emu,DNS:Goldberg Emu" ^
  -addext "keyUsage=digitalSignature,keyEncipherment" ^
  -addext "authorityKeyIdentifier=keyid,issuer:always" ^
  -addext "crlDistributionPoints=URI:Goldberg Emu" ^
  -addext "subjectKeyIdentifier=hash" ^
  -addext "issuerAltName=issuer:copy" ^
  -addext "nsComment=Goldberg Emu" ^
  -extensions v3_req
set /a exit+=%errorlevel%
if %exit% neq 0 (
  goto :end_script
)

call "%openssl_exe%" pkcs12 -export -out "%pfx_file%" -inkey "%pvt_file%" -in "%cer_file%" -passout pass:
set /a exit+=%errorlevel%
if %exit% neq 0 (
  goto :end_script
)

del /f /q "%cer_file%"
del /f /q "%pvt_file%"

call "%signtool_exe%" sign /d "Goldberg Emu" /fd sha256 /f "%pfx_file%" /p "" "%~1"
set /a exit+=%errorlevel%
if %exit% neq 0 (
  goto :end_script
)

del /f /q "%pfx_file%"

:end_script
endlocal
exit /b %exit%
