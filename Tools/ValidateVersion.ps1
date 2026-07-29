[CmdletBinding()]
param(
	[string]$HeaderPath,
	[string]$DescriptorPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$PluginRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($HeaderPath))
{
	$HeaderPath = Join-Path $PluginRoot 'Source\AngelscriptRuntime\Core\UnrealAngelscriptVersion.h'
}
if ([string]::IsNullOrWhiteSpace($DescriptorPath))
{
	$DescriptorPath = Join-Path $PluginRoot 'Angelscript.uplugin'
}

function Get-RequiredIntegerMacro
{
	param(
		[Parameter(Mandatory = $true)]
		[string]$Text,
		[Parameter(Mandatory = $true)]
		[string]$Name
	)

	$Pattern = '(?m)^\s*#define\s+' + [regex]::Escape($Name) + '\s+(?<Value>\d+)\s*$'
	$Match = [regex]::Match($Text, $Pattern)
	if (-not $Match.Success)
	{
		throw "Required integer macro '$Name' was not found."
	}

	return [int]$Match.Groups['Value'].Value
}

function Get-RequiredStringMacro
{
	param(
		[Parameter(Mandatory = $true)]
		[string]$Text,
		[Parameter(Mandatory = $true)]
		[string]$Name
	)

	$Pattern = '(?m)^\s*#define\s+' + [regex]::Escape($Name) + '\s+"(?<Value>[^"]*)"\s*$'
	$Match = [regex]::Match($Text, $Pattern)
	if (-not $Match.Success)
	{
		throw "Required string macro '$Name' was not found."
	}

	return $Match.Groups['Value'].Value
}

$ResolvedHeaderPath = (Resolve-Path -LiteralPath $HeaderPath).Path
$ResolvedDescriptorPath = (Resolve-Path -LiteralPath $DescriptorPath).Path
$HeaderText = Get-Content -LiteralPath $ResolvedHeaderPath -Raw
$Descriptor = Get-Content -LiteralPath $ResolvedDescriptorPath -Raw | ConvertFrom-Json

$Major = Get-RequiredIntegerMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_VERSION_MAJOR'
$Minor = Get-RequiredIntegerMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_VERSION_MINOR'
$Patch = Get-RequiredIntegerMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_VERSION_PATCH'
$EncodedVersion = Get-RequiredIntegerMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_VERSION'
$ProductName = Get-RequiredStringMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_PRODUCT_NAME'
$VersionString = Get-RequiredStringMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_VERSION_STRING'
$ProductVersionString = Get-RequiredStringMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING'
$UpstreamBaseVersion = Get-RequiredIntegerMacro -Text $HeaderText -Name 'UNREAL_ANGELSCRIPT_UPSTREAM_BASE_VERSION'

$ExpectedEncodedVersion = $Major * 10000 + $Minor * 100 + $Patch
$ExpectedVersionString = "$Major.$Minor.$Patch"
$ExpectedProductVersionString = "$ProductName $ExpectedVersionString"
$ValidationErrors = [System.Collections.Generic.List[string]]::new()

if ($Minor -lt 0 -or $Minor -gt 99)
{
	$ValidationErrors.Add("UNREAL_ANGELSCRIPT_VERSION_MINOR is '$Minor'; expected a value from 0 through 99.")
}
if ($Patch -lt 0 -or $Patch -gt 99)
{
	$ValidationErrors.Add("UNREAL_ANGELSCRIPT_VERSION_PATCH is '$Patch'; expected a value from 0 through 99.")
}
if ($EncodedVersion -ne $ExpectedEncodedVersion)
{
	$ValidationErrors.Add("UNREAL_ANGELSCRIPT_VERSION is '$EncodedVersion'; expected '$ExpectedEncodedVersion' from the semantic components.")
}
if ($VersionString -cne $ExpectedVersionString)
{
	$ValidationErrors.Add("UNREAL_ANGELSCRIPT_VERSION_STRING is '$VersionString'; expected '$ExpectedVersionString'.")
}
if ($ProductVersionString -cne $ExpectedProductVersionString)
{
	$ValidationErrors.Add("UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING is '$ProductVersionString'; expected '$ExpectedProductVersionString'.")
}
if ([int]$Descriptor.Version -ne $EncodedVersion)
{
	$ValidationErrors.Add("Angelscript.uplugin Version is '$($Descriptor.Version)'; expected '$EncodedVersion'.")
}
if ([string]$Descriptor.VersionName -cne $VersionString)
{
	$ValidationErrors.Add("Angelscript.uplugin VersionName is '$($Descriptor.VersionName)'; expected '$VersionString'.")
}
if ([string]$Descriptor.FriendlyName -cne $ProductName)
{
	$ValidationErrors.Add("Angelscript.uplugin FriendlyName is '$($Descriptor.FriendlyName)'; expected '$ProductName'.")
}
if ($UpstreamBaseVersion -eq $EncodedVersion)
{
	$ValidationErrors.Add("Upstream base version '$UpstreamBaseVersion' must remain distinct from product version '$EncodedVersion'.")
}

if ($ValidationErrors.Count -gt 0)
{
	foreach ($ValidationError in $ValidationErrors)
	{
		Write-Error $ValidationError -ErrorAction Continue
	}
	exit 1
}

Write-Host "Unreal AngelScript version validation passed."
Write-Host "Product: $ProductVersionString"
Write-Host "Encoded version: $EncodedVersion"
Write-Host "Header: $ResolvedHeaderPath"
Write-Host "Descriptor: $ResolvedDescriptorPath"
