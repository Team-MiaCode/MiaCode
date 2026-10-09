param(
    [Parameter(Mandatory=$true)][string]$QtHostRoot,
    [Parameter(Mandatory=$true)][string]$JdkRoot,
    [string]$QtAndroidRoot = '',
    [string]$SdkRoot = '',
    [string]$Ninja = '',
    [string]$Lrelease = '',
    [ValidateRange(-1,65535)][int]$ProxyPort = -1,
    [switch]$NativeOnly,
    [switch]$SignForTesting
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'android-toolchain.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if (!$QtAndroidRoot) { $QtAndroidRoot = Join-Path $repoRoot ".qt/$($lock.qt)/android_arm64_v8a" }
if (!$SdkRoot) { $SdkRoot = Join-Path $repoRoot '.qt/android-sdk' }
$ndkRoot = Join-Path $SdkRoot "ndk/$($lock.ndk)"
$buildDir = Join-Path $repoRoot "build-devtools/android-arm64-$($lock.qt)"
foreach ($path in @((Join-Path $QtHostRoot 'bin/moc.exe'), (Join-Path $JdkRoot 'bin/java.exe'),
    (Join-Path $ndkRoot 'build/cmake/android.toolchain.cmake'), (Join-Path $QtAndroidRoot 'lib/cmake/Qt6/qt.toolchain.cmake'))) {
    if (!(Test-Path -LiteralPath $path)) { throw "Missing Android build prerequisite: $path" }
}
$active = Get-CimInstance Win32_Process | Where-Object {
    $_.Name -match '^(cmake|ninja|MSBuild)\.exe$' -and $_.CommandLine -and $_.CommandLine.Contains($buildDir)
}
if ($active) { throw 'An attributable Android build is already running; wait for it before rebuilding.' }
$env:JAVA_HOME = $JdkRoot
$env:ANDROID_SDK_ROOT = $SdkRoot
$env:ANDROID_NDK_ROOT = $ndkRoot
$env:GRADLE_OPTS = "$env:GRADLE_OPTS -Dorg.gradle.daemon=false -Dorg.gradle.workers.max=4"
if ($ProxyPort -eq -1) {
    # Java does not automatically use Windows' local HTTP proxy. Only reuse an
    # enabled loopback proxy; do not copy credentials or change global settings.
    $proxySettings = Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings' -ErrorAction SilentlyContinue
    if ($proxySettings.ProxyEnable -and $proxySettings.ProxyServer -match '(127\.0\.0\.1|localhost):(\d+)') {
        $ProxyPort = [int]$Matches[2]
    }
}
if ($ProxyPort -gt 0) {
    $env:GRADLE_OPTS += " -Dhttp.proxyHost=127.0.0.1 -Dhttp.proxyPort=$ProxyPort -Dhttps.proxyHost=127.0.0.1 -Dhttps.proxyPort=$ProxyPort"
    Write-Output "Gradle uses the local system proxy on port $ProxyPort for dependency downloads."
}
$configure = @('-S',$repoRoot,'-B',$buildDir,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_TOOLCHAIN_FILE=$QtAndroidRoot/lib/cmake/Qt6/qt.toolchain.cmake",
    "-DQT_HOST_PATH=$QtHostRoot", "-DANDROID_SDK_ROOT=$SdkRoot", "-DANDROID_NDK=$ndkRoot",
    "-DANDROID_NDK_ROOT=$ndkRoot", '-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-31')
if ($Ninja) { $configure += "-DCMAKE_MAKE_PROGRAM=$Ninja" }
if ($Lrelease) { $configure += "-DMIACODE_LRELEASE_EXECUTABLE=$Lrelease" }
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Android configure failed' }
& cmake --build $buildDir --parallel 4 --target MiaCodeAndroid
if ($LASTEXITCODE -ne 0) { throw 'Android native build failed' }
if (!$NativeOnly) {
    # Qt generates the APK target. No signing key/password is stored in source.
    & cmake --build $buildDir --parallel 4 --target apk
    if ($LASTEXITCODE -ne 0) { throw 'Android APK packaging failed' }
    if ($SignForTesting) {
        $unsigned = Join-Path $buildDir 'android-build/build/outputs/apk/release/android-build-release-unsigned.apk'
        $signingDir = Join-Path $repoRoot 'build-devtools/android-signing'
        New-Item -ItemType Directory -Force -Path $signingDir | Out-Null
        $testKey = Join-Path $signingDir 'android-test.keystore'
        $signed = Join-Path $buildDir 'MiaCodeMobile-arm64-test.apk'
        if (!(Test-Path -LiteralPath $testKey)) {
            & (Join-Path $JdkRoot 'bin/keytool.exe') -genkeypair -keystore $testKey -alias android-test -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=MiaCode Android Test'
            if ($LASTEXITCODE -ne 0) { throw 'Test signing key creation failed' }
        }
        $signer = Join-Path $SdkRoot "build-tools/$($lock.buildTools)/apksigner.bat"
        & $signer sign --ks $testKey --ks-key-alias android-test --ks-pass pass:android --key-pass pass:android --out $signed $unsigned
        if ($LASTEXITCODE -ne 0) { throw 'Test APK signing failed' }
        & $signer verify $signed
        if ($LASTEXITCODE -ne 0) { throw 'Test APK signature verification failed' }
        Write-Output "TEST signed APK (not a release key): $signed"
    }
}
