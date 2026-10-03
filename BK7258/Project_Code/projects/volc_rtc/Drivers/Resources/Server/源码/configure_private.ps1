$ErrorActionPreference = "Stop"

function Convert-SecureText([Security.SecureString]$Value) {
    return [System.Net.NetworkCredential]::new("", $Value).Password
}

function Read-RequiredSecret([string]$Prompt, [string]$EmptyMessage) {
    while ($true) {
        $secure = Read-Host $Prompt -AsSecureString
        $plain = Convert-SecureText $secure
        if (-not [string]::IsNullOrWhiteSpace($plain)) {
            return $plain
        }
        Write-Host $EmptyMessage -ForegroundColor Yellow
    }
}

$privateDir = Join-Path $PSScriptRoot "private"
$configPath = Join-Path $privateDir "server_config.json"
$templatePath = Join-Path $privateDir "start_voice_chat_template.json"
$messagePath = Join-Path $PSScriptRoot "messages.zh-CN.json"
[System.IO.Directory]::CreateDirectory($privateDir) | Out-Null
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$messageJson = [System.IO.File]::ReadAllText($messagePath, $utf8NoBom)
$messages = $messageJson | ConvertFrom-Json
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

Write-Host $messages.title -ForegroundColor Cyan
Write-Host $messages.intro_private
Write-Host $messages.intro_hidden -ForegroundColor DarkGray
Write-Host $messages.link_tip -ForegroundColor DarkGray
Write-Host ""
Write-Host $messages.access_key_title -ForegroundColor Cyan
Write-Host $messages.access_key_step1
Write-Host $messages.access_key_step2
Write-Host $messages.access_key_step3
Write-Host $messages.access_key_url -ForegroundColor DarkCyan

$accessKeyId = Read-RequiredSecret $messages.prompt_access_key $messages.empty_value
$secretAccessKey = Read-RequiredSecret $messages.prompt_secret_key $messages.empty_value

Write-Host ""
Write-Host $messages.rtc_key_title -ForegroundColor Cyan
Write-Host $messages.rtc_key_step1
Write-Host $messages.rtc_key_step2
Write-Host $messages.rtc_key_step3
Write-Host $messages.rtc_key_step4
Write-Host $messages.rtc_key_url -ForegroundColor DarkCyan
$rtcAppKey = Read-RequiredSecret $messages.prompt_rtc_app_key $messages.empty_value

$config = [ordered]@{
    volc_access_key_id     = $accessKeyId
    volc_secret_access_key = $secretAccessKey
    rtc_app_id             = "6ab7750fd755df017a8abe2f"
    rtc_app_key            = $rtcAppKey
    rtc_api_version        = "2025-06-01"
    bind_host              = "0.0.0.0"
    port                   = 8080
}
$configJson = $config | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($configPath, $configJson, $utf8NoBom)

Write-Host ""
Write-Host $messages.code_title -ForegroundColor Cyan
Write-Host $messages.code_step1
Write-Host $messages.code_step2
Write-Host $messages.code_step3
Write-Host $messages.code_step4
Write-Host $messages.code_step5
Write-Host $messages.code_url -ForegroundColor DarkCyan
Read-Host $messages.prompt_code_ready | Out-Null

$clipboard = Get-Clipboard -Raw
$firstBrace = $clipboard.IndexOf("{")
$lastBrace = $clipboard.LastIndexOf("}")
if ($firstBrace -lt 0 -or $lastBrace -le $firstBrace) {
    throw $messages.error_no_json
}
$templateJson = $clipboard.Substring($firstBrace, $lastBrace - $firstBrace + 1)
$templateObject = $templateJson | ConvertFrom-Json
if ($null -eq $templateObject.Config) {
    throw $messages.error_no_config
}
$normalizedTemplate = $templateObject | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($templatePath, $normalizedTemplate, $utf8NoBom)

$accessKeyId = $null
$secretAccessKey = $null
$rtcAppKey = $null
$config = $null
$configJson = $null
$clipboard = $null
$templateJson = $null

Write-Host ""
Write-Host $messages.saved -ForegroundColor Green
Write-Host $messages.next_step
Read-Host $messages.close_prompt | Out-Null
