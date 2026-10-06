$ErrorActionPreference = "Stop"

function Convert-SecureText([Security.SecureString]$Value) {
    return [System.Net.NetworkCredential]::new("", $Value).Password
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$configPath = Join-Path $PSScriptRoot "private\server_config.json"
$appId = "6ab7750fd755df017a8abe2f"

if (-not [System.IO.File]::Exists($configPath)) {
    throw "没有找到 private/server_config.json，请先运行 configure_local_server.cmd。"
}

[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
Write-Host "配置小琴使用的 AI 音视频应用" -ForegroundColor Cyan
Write-Host "应用名称：defaultAIAgentAppName"
Write-Host "AppId：$appId"
Write-Host "控制台地址：" -NoNewline
Write-Host "https://console.volcengine.com/rtc/listRTC/appConfig?appId=$appId" -ForegroundColor DarkCyan
Write-Host "打开地址，点击主 AppKey 右侧的小眼睛，再点击复制。" -ForegroundColor Yellow

$secure = Read-Host "请粘贴主 AppKey（输入不会显示）" -AsSecureString
$appKey = Convert-SecureText $secure
if ([string]::IsNullOrWhiteSpace($appKey)) {
    throw "AppKey 不能为空。"
}

$configText = [System.IO.File]::ReadAllText($configPath, $utf8NoBom)
$config = $configText | ConvertFrom-Json
$config.rtc_app_id = $appId
$config.rtc_app_key = $appKey
$config.rtc_api_version = "2025-06-01"
$json = $config | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($configPath, $json, $utf8NoBom)

$appKey = $null
$secure = $null
$json = $null
Write-Host "小琴 AI 应用的 AppKey 已写入私密配置。" -ForegroundColor Green
Write-Host "请关闭旧的本地服务窗口，再双击 start_local_server.cmd。"
Read-Host "按回车关闭窗口" | Out-Null
