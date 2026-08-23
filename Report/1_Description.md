## Overview

There are two main relevant areas here, I'm reporting them together since they are part of the same system.

The first problem is a section object (file mapping) created by `NvSmartMax(64).dll` (from now on referred to as just NvSmartMax). This object is `\Sessions\1\BaseNamedObjects\SMARTMAX_Shared_Memory` and is given an open ACL providing low-integrity tokens (and above) full control. This means if the object is not yet created, a symbolic link can be created first which points to a more privileged location. This allows standard users to create an object in any location the host service (`nvcontainer.exe`) can, such as the root `\BaseNamedObjects` directory and `\Sessions\0\`. This section object is not created on system boot, and can therefore be triggered once the link is planted.

Second is a collection of issues which have to do with the parsing of this shared memory. In particular, the parsing which happens in the window hook callback functions. NvSmartMax and the callbacks are injected into any window process on the system, including high-integrity ones. As such, any issue in this parsing affects them as well. These allow for various DoS opportunities and some window manipulation.

## Triggers/Setup

I'm sure there are many ways to trigger the shared memory creation and the initialization of the hooks. I chose to programmatically use the SDK (`nvapi.h`), however, it can also be triggered with user interaction which makes testing easier in this case. The shared memory seems to be created by changing some desktop configuration such as refresh rate, scaling, etc. The hooks seem to be implemented when "Surround" (multiple monitors into one) is enabled. Using the SDK, I enable Surround which also creates the shared memory. This is just the method I found first that doesn't require user interaction. There are likely other ways.

## Section Object

First is the section object. The easiest and fastest way to demonstrate this will be with the NtObjectManager Powershell module. Once installed, use `Import-Module NtObjectManager`.

Steps to reproduce:

1. Setup
   - `Import-Module NtObjectManager`
2. Ensure objects do not already exist.
   - `ls NtObject:\Sessions\$sessionId\BaseNamedObjects | Select-String -Pattern "SMARTMAX_Shared_Memory|PoC"`
   - `ls NtObject:\BaseNamedObjects | Select-String "PoC"`
3. Obtain the current session ID.
   - `$sessionId = (Get-Process -Id $PID).SessionId`
4. Create a link from `\Sessions\$sessionId\BaseNamedObjects\SMARTMAX_Shared_Memory` to a desired location such as `\BaseNamedObjects\PoC`.
   - `$link = New-NtSymbolicLink -Path "\Sessions\$sessionId\BaseNamedObjects\SMARTMAX_Shared_Memory" -TargetPath "\BaseNamedObjects\PoC"`
5. Trigger section creation.
   - This can be done in a few ways, including programatically as demonstrated in the PoC. For testing purposes, however, I would recommend switching a monitor's refresh rate. This can be done through Windows settings or the NVIDIA App.
6. Observe the newly created object in `\BaseNamedObjects`.
   - `ls NtObject:\BaseNamedObjects\ | Select-String "PoC"`
   - `Format-NtSecurityDescriptor "\BaseNamedObjects\PoC"`

The resulting section object will have the following security:

```
Path: \BaseNamedObjects\PoC
Type: Section
Control: DaclPresent, SaclPresent, SaclAutoInherited

<Owner>
 - Name  : BUILTIN\Administrators
 - Sid   : S-1-5-32-544

<Group>
 - Name  : NT AUTHORITY\SYSTEM
 - Sid   : S-1-5-18

<DACL>
 - Type  : Allowed
 - Name  : Everyone
 - SID   : S-1-1-0
 - Mask  : 0x000F001F
 - Access: Full Access
 - Flags : None

<Mandatory Label>
 - Type  : MandatoryLabel
 - Name  : Mandatory Label\Low Mandatory Level
 - SID   : S-1-16-4096
 - Mask  : 0x00000001
 - Policy: NoWriteUp
 - Flags : None
```

As can be seen, low-integrity and above tokens have full access.

## Hooks

For the various hook issues, they target Surround mode. There are a handful of bugs when parsing the shared memory's data. The primary issue is in using one of the values within the section as a loop counter. This results in an OOB read, leading to a crash. See the PoCs and the provided solutions for more details.

These PoCs can cause quite a bit of a mess, reboots may be required between runs.

To reproduce:

1. Enable Surround mode. Provided is code to do this programmatically to prove it can be done, however, it may or may not work everywhere. It can be made to work generically. With that said, It's easier to enable it manually in the NVIDIA App.
2. Run one of the PoCs. See expected results for each below.

- CrashWindowOnMove - Provides a 5 second span of time in which any windows which are moved will crash.
- HideWindow - Will move a window off of the screen, where it is not visible.
- HideWindowTarget - This is a target program for HideWindow, useful for testing.
- Dump - This project is just my analysis of the internal structures. Primarily useful for patching, as you can map what fields I'm referring to, to the fields you have.
- Trigger - Used to enable Surround which triggers the section creation and the hooks. For testing, I found it easier to enable Surround manually when testing the PoCs. If I only needed the section creation, I would only change my refresh rate instead.

I've tried to keep these PoCs fairly controlled. Things can get quite out of hand since this effects every window on the system. I have had instances where every window is killed in a loop, and a system reboot was required.
