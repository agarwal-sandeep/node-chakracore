#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

# Init Build Script
#
# Run this as the very first step in the build to configure the environment.
# This is distinct from the Pre-Build script as there may be more non-script steps that need to be
# taken before setting up and running the build.
# For example, this script creates a cmd script which should be run to initialize environment variables.

param (
    [string]$envconfig = "ComputedEnvironment.cmd",

    [Parameter(Mandatory=$True)]
    [string]$oauth
)

$branch = $env:BUILD_SOURCEBRANCH
if (-not $branch) {
    $branch = $(git rev-parse --symbolic-full-name HEAD)
}

$BranchName = $branch.split('/',3)[2]
$BranchPath = $BranchName.replace('/','\')
$CommitHash = ${env:BUILD_SOURCEVERSION}
if (-not $CommitHash) {
    $CommitHash = $(git rev-parse HEAD)
}

$Username = $(git log $CommitHash -1 --pretty=%ae).split('@')[0]
$CommitDateTime = [DateTime]$(git log $CommitHash -1 --pretty=%aD)
$CommitTime = Get-Date $CommitDateTime -Format yyMMdd.HHmm

#
# (borrowed from pre_build.ps1)
# Get PushID and PushDate from VSO
# TODO (doilij) refactor this into a method in a util script.
#

# Get the git remote path and construct the rest API URI
$remote = (iex "git remote -v")[0].split()[1].replace("_git", "_apis/git/repositories");
$remote = $remote.replace("mshttps", "https")

# Get the pushId and push date time to use that for build number and build date time
$uri = ("{0}/commits/{1}?api-version=1.0" -f $remote, $commitHash)
$oauthToken = Get-Content $oauth
$header = @{Authorization=("Basic {0}" -f $oauthToken) }
$info = Invoke-RestMethod -Headers $header -Uri $uri -Method GET

$BuildPushDate = [datetime]$info.push.date
$PushDate = Get-Date $BuildPushDate -Format yyMMdd.HHmm
$buildPushId = $info.push.pushId
$buildPushIdPart1 = [int]([math]::Floor($buildPushId / 65536))
$buildPushIdPart2 = [int]($buildPushId % 65536)

$PushID = "{0}.{1}" -f $buildPushIdPart1.ToString("00000"), $buildPushIdPart2.ToString("00000")
$FullVersionString = "${Env:VERSION_MAJOR}.${Env:VERSION_MINOR}.${PushID}"

#
# (end code borrowed from pre_build.ps1)
#

# unless it is a build branch, subdivide the output directory by month
if ($BranchPath.StartsWith("build")) {
    $YearAndMonth = ""
} else {
    $YearAndMonth = Get-Date $BuildPushDate -Format yyMM
}

$YearAndMonthSeparator = ""
if ($YearAndMonth) {
    $YearAndMonthSeparator = "\"
}

$BuildIdentifier = "${PushID}_${PushDate}_${Username}_${CommitHash}"
$OutputPath = "${BranchPath}\${YearAndMonth}${YearAndMonthSeparator}${BuildIdentifier}"
$FlavorName = "${Env:BuildPlatform}_${Env:BuildConfiguration}"

# Create a sentinel file for each build flavor and for the overall build
# to track whether the build is complete.
# * build.incomplete                   # will be deleted when the release task completes
# * ${arch}_${flavor}.incomplete       # will be deleted when the build of this flavor completes

$buildIncompleteFileContentsString = @"
{0} in progress.
The contents of this directory should not be relied on until the build completes.
"@

$FullOutputPath = Join-Path ${env:DROP_ROOT} ${OutputPath}
New-Item -ItemType Directory -Force -Path $FullOutputPath
New-Item -ItemType Directory -Force -Path (Join-Path $Env:BUILD_BINARIESDIRECTORY "buildlogs")
New-Item -ItemType Directory -Force -Path (Join-Path $Env:BUILD_BINARIESDIRECTORY "logs")

$buildIncompleteFile = Join-Path $FullOutputPath "build.incomplete"
$flavorBuildIncompleteFile = Join-Path $FullOutputPath "${FlavorName}.incomplete"

if (-not (Test-Path $buildIncompleteFile)) {
    ($buildIncompleteFileContentsString -f "Build") `
        | Out-File $buildIncompleteFile -Encoding Ascii
}
if (-not (Test-Path $flavorBuildIncompleteFile)) {
    ($buildIncompleteFileContentsString -f "Build of ${FlavorName}") `
        | Out-File $flavorBuildIncompleteFile -Encoding Ascii
}

# Write the $envconfig script.

@"
set BranchName=${BranchName}
set BranchPath=${BranchPath}
set YearAndMonth=${YearAndMonth}
set BuildIdentifier=${BuildIdentifier}

set PushID=${PushID}
set FullVersionString=${FullVersionString}
set PushDate=${PushDate}
set CommitTime=${CommitTime}
set Username=${Username}
set CommitHash=${CommitHash}

set OutputPath=${OutputPath}
set FlavorName=${FlavorName}

set BuildIncompleteFile=${buildIncompleteFile}
set FlavorBuildIncompleteFile=${flavorBuildIncompleteFile}
"@ `
    | Out-File $envconfig -Encoding Ascii

# Use the MBv2 environment to construct a MBv1 VSO environment
# for the sake of reusing the pre-build and post-build scripts as they are.

@"
set TF_BUILD_SOURCEGETVERSION=LG:${branch}:${CommitHash}
set TF_BUILD_DROPLOCATION=${Env:BUILD_BINARIESDIRECTORY}

set TF_BUILD_BUILDDEFINITIONNAME=${Env:BUILD_DEFINITIONNAME}
set TF_BUILD_BUILDNUMBER=${Env:BUILD_BUILDNUMBER}
set TF_BUILD_BUILDURI=${Env:BUILD_BUILDURI}
"@ `
    | Out-File $envconfig -Encoding Ascii -Append

# Set VSO variables that can be consumed by other VSO tasks.
# Uses command syntax documented here:
# https://github.com/Microsoft/vso-agent-tasks/blob/master/docs/authoring/commands.md
# Lines written to stdout that match this pattern are interpreted with this command syntax.

Write-Output "Setting VSO variable VSO_OutputPath = ${OutputPath}"
Write-Output "##vso[task.setvariable variable=VSO_OutputPath;]${OutputPath}"

Write-Output "Setting VSO variable VSO_FullVersionString = ${FullVersionString}"
Write-Output "##vso[task.setvariable variable=VSO_FullVersionString;]${FullVersionString}"

# TODO (doilij): move this up and assign values

# Inferable Environment (if not specified, inferred by pre_post_util.ps1):
#   $Env:TF_BUILD_SOURCESDIRECTORY  (a.k.a. $srcpath)
#   $Env:TF_BUILD_BUILDDIRECTORY    (a.k.a. $objpath)
#   $Env:TF_BUILD_BINARIESDIRECTORY (a.k.a. $binpath)
