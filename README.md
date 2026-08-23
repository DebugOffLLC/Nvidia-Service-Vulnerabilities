# NVSmartMax Vulnerabilities

Nvidia was notified on May 30th, 2026 of several issues with the NVSmartMax service. The triage team (not Nvidia) was unable to replicate the issues, likely due to their virtualized testing environment. The issue was forwarded to Nvidia on June 24th 2026. Their first and only response was on July 1st 2026. Weeks after this initial response there was no further communication from Nvidia. As such, on July 21st, our researcher reached out asking for a status update. No further updates were provided. Our researcher reached out to them once again on August 13th notifying them of our intent to disclose the issue at the 90 day mark (August 30th) unless they provided a further update. No further update was received.

In short, after Nvidia acknowledged the report on July 1st, they never responded or followed up again. Seeing as the report had been in for three months, and not hearing back after two months, we've decided to disclose some of the issues publicly.

## Limitations

While our research team was able to reproduce the issues noted in this repository on several different systems, the initial triage team was not. We believe this was due to a virtualized environment. This is elaborated on more in the vulnerability writeup. Regardless, Nvidia themselves should've been able to triage it, but as far as we are aware they never even tried.

The vulnerabilities provided are not complete, merely PoCs.

## Vulnerabilities TL;DR

> For a more full report, see the redacted initial report which was provided to NVidia in the [Report/](Report/) folder.

### LPE

The primary vulnerability of concern has to due with improper access controls on a shared section object (shared memory). It is possible to preemptively put a link on the object, leading to a low integrity user gaining access to high integrity objects. This allows a low integrity user to interfere with high integrity processes (other services, drivers, etc.). This also crosses the session boundary.

Exploitation of this vulnerability would allow a low integrity user to gain SYSTEM level access.

### System Instability

The shared memory object used by the system service is user-writeable. The memory is also incorrectly parsed by the DLL which can easily lead to crashing any, or all, windowed processes. Simply filling the shared memory with bytes of 0xFF will lead to every windowed process, across all sessions, crashing. The system becomes unusable.

With more precision, it is possible to control (and crash) individual windows. This includes resizing them and moving them. This makes some forms of cross-session window hijacking possible.

### Spread & Impact

The stability issues impact every windowed process across all sessions, not just Nvidia ones. The vulnerable DLL contains window hooks. These hooks are installed by the high integrity Nvidia services. As such, every windowed process of all integrity levels will have this vulnerable and buggy DLL injected into them by the OS. This is what allows the controlling of every window and the causation of full system instability.

## Report

You can find a redacted version of the initial report which was provided to Nvidia in the [Report/](Report/) folder.
