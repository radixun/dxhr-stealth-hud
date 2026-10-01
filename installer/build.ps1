$ErrorActionPreference='Stop'
$sourceDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$payload=Join-Path $sourceDir 'payload'
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
$argsList=@('/nologo','/target:winexe','/platform:x86','/optimize+','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Xml.Linq.dll',('/out:'+(Join-Path $sourceDir '../StealthHUD-Setup-0.4.0.exe')))
foreach($file in Get-ChildItem -LiteralPath $payload -File){$argsList+='/resource:'+$file.FullName+',payload.'+$file.Name}
$argsList+=Join-Path $sourceDir 'Setup.cs'
& $csc $argsList
if($LASTEXITCODE -ne 0){throw 'Build failed'}

