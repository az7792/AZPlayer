# ==========================================
#              AZPlayer 发布脚本
# ==========================================

function Stop-WithPause {
    param([int]$ExitCode = 0)
    Write-Host "`n按任意键退出..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit $ExitCode
}

# 获取脚本所在的根目录
$RootDir = $PSScriptRoot
Set-Location $RootDir

# 从 config/env.json 读取配置
$ConfigFile = Join-Path $RootDir "config/env.json"

if (-not (Test-Path $ConfigFile)) {
    Write-Host "--- 配置错误 ---" -ForegroundColor Red
    Write-Host "未找到配置文件: config/env.json" -ForegroundColor Yellow
    Write-Host "请参考README并创建配置文件。" -ForegroundColor Cyan
    Stop-WithPause 1
}

try {
    $Config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
} catch {
    Write-Host "--- 配置错误 ---" -ForegroundColor Red
    Write-Host "无法解析 config/env.json，请检查 JSON 格式是否正确。" -ForegroundColor Yellow
    Write-Error $_
    Stop-WithPause 1
}

# 辅助函数：将相对路径转为绝对路径
function Get-AbsolutePath($Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

# 从配置中读取路径
$ExeSourcePath = $Config.TARGET_EXE_PATH
$LibAssBinPath = Join-Path $Config.ASS_DIR "bin"
$FfmpegBinPath = Join-Path $Config.FFMPEG_DIR "bin"
$WindeployqtPath = $Config.WINDEPLOYQT_EXE

$FullExePath = Get-AbsolutePath $ExeSourcePath
$FullLibAssPath = Get-AbsolutePath $LibAssBinPath
$FullFfmpegPath = Get-AbsolutePath $FfmpegBinPath
$FullQtPath = Get-AbsolutePath $WindeployqtPath

# 1. 初始化检查
$MissingConfigs = @()
if ([string]::IsNullOrWhiteSpace($ExeSourcePath)) { $MissingConfigs += "TARGET_EXE_PATH" }
if ([string]::IsNullOrWhiteSpace($LibAssBinPath)) { $MissingConfigs += "ASS_DIR" }
if ([string]::IsNullOrWhiteSpace($FfmpegBinPath)) { $MissingConfigs += "FFMPEG_DIR" }
if ([string]::IsNullOrWhiteSpace($WindeployqtPath)) { $MissingConfigs += "WINDEPLOYQT_EXE" }

if ($MissingConfigs.Count -gt 0) {
    Write-Host "--- 配置错误 ---" -ForegroundColor Red
    $MissingConfigs | ForEach-Object { Write-Host "缺少配置: $_" -ForegroundColor Yellow }
    Write-Host "请检查 config/env.json 文件。" -ForegroundColor Cyan
    Stop-WithPause 1
}

# 2. 目录准备
$ReleaseDir = Join-Path $RootDir "release"
if (-not (Test-Path $ReleaseDir)) {
    New-Item -ItemType Directory -Path $ReleaseDir | Out-Null
}

# 3. 拷贝 LICENSES
Write-Host "正在拷贝 LICENSES 文件夹..." -ForegroundColor Cyan

$LicenseSource = Join-Path $RootDir "LICENSES"
$LicenseDest = Join-Path $ReleaseDir "LICENSES"

if (Test-Path $LicenseSource) {
    # 递归拷贝整个文件夹及其内容
    Copy-Item -Path $LicenseSource -Destination $LicenseDest -Recurse -Force
    Write-Host "LICENSES 拷贝成功。" -ForegroundColor Gray
} else {
    Write-Warning "未在当前目录下找到 LICENSES 文件夹。" -ForegroundColor Red
    exiStop-WithPauset 1
}

# 4. 拷贝主程序
$ExeName = Split-Path $FullExePath -Leaf
$DestExePath = Join-Path $ReleaseDir $ExeName
Write-Host "正在拷贝程序: $ExeName" -ForegroundColor Cyan
Copy-Item -Path $FullExePath -Destination $DestExePath -Force

# 5. 拷贝 LibAss (所有 .dll)
Write-Host "正在同步 LibAss 库..." -ForegroundColor Cyan
Get-ChildItem -Path $FullLibAssPath -Filter "*.dll" | Copy-Item -Destination $ReleaseDir -Force

# 6. 拷贝 FFmpeg (指定 .dll)
Write-Host "正在同步 FFmpeg 库..." -ForegroundColor Cyan
$FfmpegPatterns = @("avcode*.dll", "avformat*.dll", "avutil*.dll", "swresample*.dll", "swscale*.dll")
foreach ($pattern in $FfmpegPatterns) {
    # 构造完整的源文件匹配路径
    $SourcePath = Join-Path $FullFfmpegPath $pattern
    
    if (Test-Path $SourcePath) {
        # 如果找到了，执行拷贝
        Copy-Item -Path $SourcePath -Destination $ReleaseDir -Force
    } else {
        # 如果没找到，打印红色错误并终止脚本
        Write-Host "------------------------------------------" -ForegroundColor Red
        Write-Host "错误：在 FFmpeg 目录中未找到关键组件 [$pattern]" -ForegroundColor Red
        Write-Host "搜索路径: $FullFfmpegPath" -ForegroundColor Yellow
        Write-Host "请检查 FFmpeg 目录配置是否正确或文件是否缺失。" -ForegroundColor Cyan
        Stop-WithPause 1  # 终止脚本运行
    }
}

# 7. 运行打包工具
Write-Host "正在运行 windeployqt 打包依赖..." -ForegroundColor Green
$QmlDir = Join-Path $RootDir "qml"

try {
    # 构造并执行命令
    & $FullQtPath --qmldir "$QmlDir" "$DestExePath"
    Write-Host "`n------------------------------------------" -ForegroundColor Green
    Write-Host "           发布完成！目录: ./release" -ForegroundColor Green
    Write-Host "------------------------------------------" -ForegroundColor Green
} catch {
    Write-Host "打包过程中出现意外错误。" -ForegroundColor Red
    Write-Error $_
}

Stop-WithPause 0