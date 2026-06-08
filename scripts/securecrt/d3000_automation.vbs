#$language = "VBScript"
#$interface = "1.0"

Option Explicit

Const LOGIN_USER = "root"
Const LOGIN_PASSWORD = "123456"

Const PRIMARY_PROMPT = "# "
Const SECONDARY_PROMPT = "$ "

Const DEFAULT_COMMAND_TIMEOUT = 30
Const DEFAULT_LOGIN_TIMEOUT = 180
Const DEFAULT_BOOT_TIMEOUT = 180
Const DEFAULT_LOG_DIR = "C:\SecureCRTLogs"
Const PLAN_SEP = "@@"

Const USE_SUDO = False
Const REBOOT_CMD = "/sbin/reboot"
Const SOFT_RESET_CMD = "sh -c 'echo 1 > /proc/sys/kernel/sysrq; echo b > /proc/sysrq-trigger'"

' POWER_PROVIDER:
'   http    = use HTTP power control API
'   command = use local command such as ipmitool, wolcmd or powershell
'   none    = power on/off actions are disabled
Const POWER_PROVIDER = "http"

Const POWER_HTTP_METHOD = "POST"
Const POWER_HTTP_TOKEN = ""
Const POWER_ON_URL = "http://192.168.10.50/api/power/on"
Const POWER_OFF_URL = "http://192.168.10.50/api/power/off"
Const POWER_CYCLE_URL = "http://192.168.10.50/api/power/cycle"

Const POWER_ON_CMD = ""
Const POWER_OFF_CMD = ""
Const POWER_CYCLE_CMD = ""

Main

Sub Main()
    crt.Screen.Synchronous = True

    Dim choice
    choice = Trim(crt.Dialog.Prompt( _
        "Select action:" & vbCrLf & _
        "1 = Health check" & vbCrLf & _
        "2 = Single reboot" & vbCrLf & _
        "3 = Single soft reset" & vbCrLf & _
        "4 = Power on" & vbCrLf & _
        "5 = Power cycle" & vbCrLf & _
        "6 = Reboot stability test" & vbCrLf & _
        "7 = CPU/memory stress test" & vbCrLf & _
        "8 = Load plan from URL", _
        "SecureCRT D3000 Automation", "1", False))

    If choice = "" Then Exit Sub

    StartDefaultLog
    EnsureLoggedIn DEFAULT_LOGIN_TIMEOUT

    Select Case choice
        Case "1"
            HealthCheck

        Case "2"
            RebootTarget DEFAULT_BOOT_TIMEOUT

        Case "3"
            SoftResetTarget DEFAULT_BOOT_TIMEOUT

        Case "4"
            PowerOnTarget DEFAULT_BOOT_TIMEOUT

        Case "5"
            PowerCycleTarget 5, DEFAULT_BOOT_TIMEOUT

        Case "6"
            Dim rounds
            rounds = ToInt(crt.Dialog.Prompt("Input reboot rounds", "Reboot stability", "50", False), 50)
            RebootLoop rounds, DEFAULT_BOOT_TIMEOUT

        Case "7"
            Dim stressSeconds
            stressSeconds = ToInt(crt.Dialog.Prompt("Input stress seconds", "Stress test", "600", False), 600)
            CpuMemStress stressSeconds

        Case "8"
            Dim planUrl
            planUrl = Trim(crt.Dialog.Prompt("Input plan URL", "Load test plan", "http://127.0.0.1/d3000_plan.txt", False))
            If planUrl <> "" Then
                RunPlanFromUrl planUrl
            End If

        Case Else
            crt.Dialog.MessageBox "Unknown option: " & choice
    End Select

    crt.Session.SetStatusText "Script finished"
End Sub

Sub StartDefaultLog()
    Dim logFile
    logFile = DEFAULT_LOG_DIR & "\d3000_" & BuildTimeStamp() & ".log"
    EnsureFolder DEFAULT_LOG_DIR

    If crt.Session.Logging Then
        crt.Session.Log False
    End If

    crt.Session.LogFileName = logFile
    crt.Session.Log True
    crt.Session.SetStatusText "Logging to: " & logFile
End Sub

Sub StartNamedLog(logFile)
    Dim parentFolder
    parentFolder = GetParentFolder(logFile)
    If parentFolder <> "" Then
        EnsureFolder parentFolder
    End If

    If crt.Session.Logging Then
        crt.Session.Log False
    End If

    crt.Session.LogFileName = logFile
    crt.Session.Log True
    crt.Session.SetStatusText "Logging to: " & logFile
End Sub

Sub EnsureFolder(folderPath)
    Dim fso
    Set fso = CreateObject("Scripting.FileSystemObject")

    If folderPath = "" Then Exit Sub

    If fso.FolderExists(folderPath) Then Exit Sub

    Dim parentFolder
    parentFolder = fso.GetParentFolderName(folderPath)
    If parentFolder <> "" And Not fso.FolderExists(parentFolder) Then
        EnsureFolder parentFolder
    End If

    If Not fso.FolderExists(folderPath) Then
        fso.CreateFolder folderPath
    End If
End Sub

Sub EnsureLoggedIn(timeoutSec)
    Dim idx

    crt.Screen.Send vbCr
    idx = crt.Screen.WaitForStrings(Array("login:", "Login:", "Password:", PRIMARY_PROMPT, SECONDARY_PROMPT), timeoutSec)

    Select Case idx
        Case 1, 2
            crt.Screen.Send LOGIN_USER & vbCr
            If crt.Screen.WaitForString("Password:", 15) Then
                crt.Screen.Send LOGIN_PASSWORD & vbCr
            End If

            If Not WaitForShellPrompt(timeoutSec) Then
                AbortScript "Login failed or prompt not found"
            End If

        Case 3
            crt.Screen.Send LOGIN_PASSWORD & vbCr
            If Not WaitForShellPrompt(timeoutSec) Then
                AbortScript "Prompt not found after password"
            End If

        Case 4, 5
            ' already at shell prompt

        Case Else
            AbortScript "Timeout waiting for login prompt"
    End Select
End Sub

Function WaitForShellPrompt(timeoutSec)
    Dim idx
    idx = crt.Screen.WaitForStrings(Array(PRIMARY_PROMPT, SECONDARY_PROMPT), timeoutSec)
    WaitForShellPrompt = (idx > 0)
End Function

Function WaitForTargetReady(timeoutSec)
    Dim elapsed
    Dim idx

    elapsed = 0

    Do While elapsed < timeoutSec
        crt.Screen.Send vbCr
        idx = crt.Screen.WaitForStrings(Array("login:", "Login:", "Password:", PRIMARY_PROMPT, SECONDARY_PROMPT), 5)

        If idx > 0 Then
            EnsureLoggedIn DEFAULT_LOGIN_TIMEOUT
            WaitForTargetReady = True
            Exit Function
        End If

        elapsed = elapsed + 5
    Loop

    WaitForTargetReady = False
End Function

Function RunCommandCapture(cmdText, timeoutSec)
    crt.Screen.Send cmdText & vbCr
    RunCommandCapture = crt.Screen.ReadString(Array(PRIMARY_PROMPT, SECONDARY_PROMPT), timeoutSec)
End Function

Sub RunCommandNoWait(cmdText)
    crt.Screen.Send cmdText & vbCr
End Sub

Sub HealthCheck()
    crt.Session.SetStatusText "Running health check"

    RunCommandCapture "echo ===== BASIC INFO =====", 10
    RunCommandCapture "date", 10
    RunCommandCapture "uname -a", 10
    RunCommandCapture "cat /etc/os-release 2>/dev/null || lsb_release -a 2>/dev/null", 15
    RunCommandCapture "uptime", 10
    RunCommandCapture "free -m", 10
    RunCommandCapture "df -h", 15
    RunCommandCapture "ip addr", 15
    RunCommandCapture "dmesg | tail -n 50", 20
End Sub

Sub RebootTarget(bootTimeout)
    crt.Session.SetStatusText "Executing reboot"
    RunCommandNoWait BuildPrivCmd(REBOOT_CMD)
    WaitSeconds 3

    If Not WaitForTargetReady(bootTimeout) Then
        AbortScript "Target did not recover after reboot"
    End If

    HealthCheck
End Sub

Sub SoftResetTarget(bootTimeout)
    crt.Session.SetStatusText "Executing soft reset"
    RunCommandNoWait BuildPrivCmd(SOFT_RESET_CMD)
    WaitSeconds 3

    If Not WaitForTargetReady(bootTimeout) Then
        AbortScript "Target did not recover after soft reset"
    End If

    HealthCheck
End Sub

Sub PowerOnTarget(bootTimeout)
    crt.Session.SetStatusText "Executing power on"

    If Not ExecutePowerAction("on") Then
        AbortScript "Power-on action failed"
    End If

    If Not WaitForTargetReady(bootTimeout) Then
        AbortScript "Target did not recover after power-on"
    End If

    HealthCheck
End Sub

Sub PowerCycleTarget(offDelaySec, bootTimeout)
    crt.Session.SetStatusText "Executing power cycle"

    If Not ExecutePowerAction("cycle") Then
        AbortScript "Power-cycle action failed"
    End If

    WaitSeconds offDelaySec

    If Not WaitForTargetReady(bootTimeout) Then
        AbortScript "Target did not recover after power cycle"
    End If

    HealthCheck
End Sub

Sub RebootLoop(rounds, bootTimeout)
    Dim i

    For i = 1 To rounds
        crt.Session.SetStatusText "Reboot loop: round " & CStr(i) & " / " & CStr(rounds)
        RebootTarget bootTimeout
        RunCommandCapture "echo ROUND=" & CStr(i), 10
        RunCommandCapture "uptime", 10
    Next
End Sub

Sub CpuMemStress(seconds)
    Dim cmdText

    crt.Session.SetStatusText "Running stress test"

    cmdText = "if command -v stress-ng >/dev/null 2>&1; then " & _
              "stress-ng --cpu 4 --io 1 --vm 1 --vm-bytes 256M --timeout " & CStr(seconds) & "s --metrics-brief; " & _
              "elif command -v stress >/dev/null 2>&1; then " & _
              "stress --cpu 4 --io 1 --vm 1 --timeout " & CStr(seconds) & "; " & _
              "else echo 'stress-ng/stress not installed, skip'; fi"

    RunCommandCapture cmdText, seconds + 60
    HealthCheck
End Sub

Sub PingCheck(hostName, count, timeoutSec)
    RunCommandCapture "ping -c " & CStr(count) & " " & hostName, timeoutSec
End Sub

Sub RunPlanFromUrl(planUrl)
    Dim responseText

    crt.Session.SetStatusText "Loading test plan from URL"

    If Not HttpCall("GET", planUrl, "", responseText) Then
        AbortScript "Failed to load test plan: " & planUrl
    End If

    RunPlanText responseText
End Sub

Sub RunPlanText(planText)
    Dim normalized
    Dim lines
    Dim i
    Dim lineText

    normalized = Replace(planText, vbCrLf, vbLf)
    normalized = Replace(normalized, vbCr, vbLf)
    lines = Split(normalized, vbLf)

    For i = 0 To UBound(lines)
        lineText = Trim(lines(i))

        If lineText <> "" Then
            If Left(lineText, 1) <> "#" Then
                crt.Session.SetStatusText "Executing plan line: " & lineText
                ExecutePlanLine lineText
            End If
        End If
    Next
End Sub

Sub ExecutePlanLine(lineText)
    Dim parts
    Dim actionName

    parts = Split(lineText, PLAN_SEP)
    actionName = UCase(Trim(GetArg(parts, 0, "")))

    Select Case actionName
        Case "LOG_START"
            StartNamedLog ReplaceMacros(GetArg(parts, 1, DEFAULT_LOG_DIR & "\plan_" & BuildTimeStamp() & ".log"))

        Case "LOG_STOP"
            If crt.Session.Logging Then
                crt.Session.Log False
            End If

        Case "CHECK_LOGIN"
            EnsureLoggedIn ToInt(GetArg(parts, 1, CStr(DEFAULT_LOGIN_TIMEOUT)), DEFAULT_LOGIN_TIMEOUT)

        Case "RUN"
            RunCommandCapture GetArg(parts, 1, ""), ToInt(GetArg(parts, 2, CStr(DEFAULT_COMMAND_TIMEOUT)), DEFAULT_COMMAND_TIMEOUT)

        Case "EXPECT"
            AssertContains GetArg(parts, 1, ""), GetArg(parts, 2, ""), ToInt(GetArg(parts, 3, CStr(DEFAULT_COMMAND_TIMEOUT)), DEFAULT_COMMAND_TIMEOUT)

        Case "REBOOT"
            RebootTarget ToInt(GetArg(parts, 1, CStr(DEFAULT_BOOT_TIMEOUT)), DEFAULT_BOOT_TIMEOUT)

        Case "RESET"
            SoftResetTarget ToInt(GetArg(parts, 1, CStr(DEFAULT_BOOT_TIMEOUT)), DEFAULT_BOOT_TIMEOUT)

        Case "POWER_ON"
            PowerOnTarget ToInt(GetArg(parts, 1, CStr(DEFAULT_BOOT_TIMEOUT)), DEFAULT_BOOT_TIMEOUT)

        Case "POWER_CYCLE"
            PowerCycleTarget ToInt(GetArg(parts, 1, "5"), 5), ToInt(GetArg(parts, 2, CStr(DEFAULT_BOOT_TIMEOUT)), DEFAULT_BOOT_TIMEOUT)

        Case "HEALTH_CHECK"
            HealthCheck

        Case "REBOOT_LOOP"
            RebootLoop ToInt(GetArg(parts, 1, "10"), 10), ToInt(GetArg(parts, 2, CStr(DEFAULT_BOOT_TIMEOUT)), DEFAULT_BOOT_TIMEOUT)

        Case "STRESS_CPU"
            CpuMemStress ToInt(GetArg(parts, 1, "600"), 600)

        Case "PING"
            PingCheck GetArg(parts, 1, "127.0.0.1"), ToInt(GetArg(parts, 2, "10"), 10), ToInt(GetArg(parts, 3, "60"), 60)

        Case "WAIT"
            WaitSeconds ToInt(GetArg(parts, 1, "5"), 5)

        Case "HTTP_GET"
            AssertHttpGet GetArg(parts, 1, ""), GetArg(parts, 2, "")

        Case Else
            AbortScript "Unknown plan action: " & actionName
    End Select
End Sub

Sub AssertContains(cmdText, expectText, timeoutSec)
    Dim outputText

    outputText = RunCommandCapture(cmdText, timeoutSec)

    If InStr(1, outputText, expectText, vbTextCompare) = 0 Then
        AbortScript "Expected text not found. Command=" & cmdText & ", expected=" & expectText
    End If
End Sub

Sub AssertHttpGet(targetUrl, expectText)
    Dim responseText

    If Not HttpCall("GET", targetUrl, "", responseText) Then
        AbortScript "HTTP GET failed: " & targetUrl
    End If

    If expectText <> "" Then
        If InStr(1, responseText, expectText, vbTextCompare) = 0 Then
            AbortScript "HTTP response does not contain expected text: " & expectText
        End If
    End If
End Sub

Function ExecutePowerAction(actionName)
    Dim responseText
    Dim cmdText

    Select Case LCase(POWER_PROVIDER)
        Case "http"
            ExecutePowerAction = HttpCall(POWER_HTTP_METHOD, GetPowerUrl(actionName), "", responseText)

        Case "command"
            cmdText = GetPowerCmd(actionName)
            If cmdText = "" Then
                ExecutePowerAction = False
            Else
                ExecutePowerAction = (RunLocalHidden(cmdText) = 0)
            End If

        Case Else
            ExecutePowerAction = False
    End Select
End Function

Function GetPowerUrl(actionName)
    Select Case LCase(actionName)
        Case "on"
            GetPowerUrl = POWER_ON_URL
        Case "off"
            GetPowerUrl = POWER_OFF_URL
        Case "cycle"
            GetPowerUrl = POWER_CYCLE_URL
        Case Else
            GetPowerUrl = ""
    End Select
End Function

Function GetPowerCmd(actionName)
    Select Case LCase(actionName)
        Case "on"
            GetPowerCmd = POWER_ON_CMD
        Case "off"
            GetPowerCmd = POWER_OFF_CMD
        Case "cycle"
            GetPowerCmd = POWER_CYCLE_CMD
        Case Else
            GetPowerCmd = ""
    End Select
End Function

Function HttpCall(httpMethod, url, body, ByRef responseText)
    On Error Resume Next

    Dim xhr
    Set xhr = CreateObject("MSXML2.XMLHTTP")

    xhr.Open httpMethod, url, False

    If POWER_HTTP_TOKEN <> "" Then
        xhr.setRequestHeader "Authorization", "Bearer " & POWER_HTTP_TOKEN
    End If

    If body <> "" Then
        xhr.setRequestHeader "Content-Type", "application/json"
    End If

    xhr.Send body

    If Err.Number <> 0 Then
        responseText = Err.Description
        HttpCall = False
        Err.Clear
        On Error GoTo 0
        Exit Function
    End If

    responseText = xhr.responseText
    HttpCall = (xhr.Status >= 200 And xhr.Status < 300)

    On Error GoTo 0
End Function

Function RunLocalHidden(cmdText)
    Dim shell
    Set shell = CreateObject("WScript.Shell")
    RunLocalHidden = shell.Run(cmdText, 0, True)
End Function

Function BuildPrivCmd(cmdText)
    If USE_SUDO Then
        BuildPrivCmd = "sudo " & cmdText
    Else
        BuildPrivCmd = cmdText
    End If
End Function

Function GetArg(parts, idx, defaultValue)
    If IsArray(parts) Then
        If UBound(parts) >= idx Then
            GetArg = Trim(parts(idx))
            Exit Function
        End If
    End If

    GetArg = defaultValue
End Function

Function ToInt(textValue, defaultValue)
    If Trim(CStr(textValue)) = "" Then
        ToInt = defaultValue
    ElseIf IsNumeric(textValue) Then
        ToInt = CInt(textValue)
    Else
        ToInt = defaultValue
    End If
End Function

Function ReplaceMacros(textValue)
    ReplaceMacros = Replace(textValue, "%TIMESTAMP%", BuildTimeStamp())
End Function

Function BuildTimeStamp()
    Dim d
    d = Now

    BuildTimeStamp = Year(d) & _
                     Right("0" & Month(d), 2) & _
                     Right("0" & Day(d), 2) & "_" & _
                     Right("0" & Hour(d), 2) & _
                     Right("0" & Minute(d), 2) & _
                     Right("0" & Second(d), 2)
End Function

Function GetParentFolder(filePath)
    Dim fso
    Set fso = CreateObject("Scripting.FileSystemObject")
    GetParentFolder = fso.GetParentFolderName(filePath)
End Function

Sub WaitSeconds(seconds)
    Dim i
    For i = 1 To seconds
        crt.Sleep 1000
    Next
End Sub

Sub AbortScript(msgText)
    crt.Session.SetStatusText "Failed: " & msgText
    crt.Dialog.MessageBox msgText, "Script aborted"
    Err.Raise vbObjectError + 513, "D3000Automation", msgText
End Sub