## Section Object

Low-integrity and normal user (medium-integrity) processes can hijack the objects of high-integrity processes, services, and drivers. They often rely on the trust boundaries established by object directory permissions. This can lead to LPE by hijacking objects which contain pointers, paths to DLLs to be loaded, CLSIDs, and more.

## Hook Callbacks

Users can plant malicious values into the shared memory. This makes it possible to crash a wide range of processes, including high-integrity ones. In the best case, this causes the system to become unstable and requiring a reboot. In the worst case, critical system components could be targeted causing a loss of system integrity.
