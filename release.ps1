# ==========================================
#              用户配置区域(需要全部配置)
# ==========================================

# [常量1] 编译后的可执行文件路径
$ExeSourcePath = ".\out\build\release\appAZPlayer.exe"

# [常量2] libass/bin 的文件夹路径
$LibAssBinPath = ".\libass\bin"

# [常量3] ffmpeg/bin 的文件夹路径
$FfmpegBinPath = "D:\ffmpeg-8.0-full_build-shared\bin"

# [常量4] windeployqt.exe 所在路径
# !!! 提示：请确保此工具与编译主程序时使用的套件一致 (如均为 MinGW 或均为 MSVC)
$WindeployqtPath = "D:\Qt\6.6.3\msvc2019_64\bin\windeployqt.exe"

# ==========================================
#              自动转换与检查逻辑
# ==========================================

# 获取脚本所在的根目录
$RootDir = $PSScriptRoot
Set-Location $RootDir

# 辅助函数：将相对路径转为绝对路径
function Get-AbsolutePath($Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

$FullExePath = Get-AbsolutePath $ExeSourcePath
$FullLibAssPath = Get-AbsolutePath $LibAssBinPath
$FullFfmpegPath = Get-AbsolutePath $FfmpegBinPath
$FullQtPath = Get-AbsolutePath $WindeployqtPath

# 1. 初始化检查
$MissingConfigs = @()
if ([string]::IsNullOrWhiteSpace($ExeSourcePath)) { $MissingConfigs += "常量1: 可执行文件路径" }
if ([string]::IsNullOrWhiteSpace($LibAssBinPath)) { $MissingConfigs += "常量2: libass 路径" }
if ([string]::IsNullOrWhiteSpace($FfmpegBinPath)) { $MissingConfigs += "常量3: ffmpeg 路径" }
if ([string]::IsNullOrWhiteSpace($WindeployqtPath)) { $MissingConfigs += "常量4: windeployqt 路径" }

if ($MissingConfigs.Count -gt 0) {
    Write-Host "--- 配置错误 ---" -ForegroundColor Red
    $MissingConfigs | ForEach-Object { Write-Host "缺少配置: $_" -ForegroundColor Yellow }
    Write-Host "请编辑脚本顶部常量后再运行。" -ForegroundColor Cyan
    exit 1
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
    exit 1
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
        exit 1  # 终止脚本运行
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