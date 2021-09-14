Vulnerability PoCs of Android Bluetoodh.

avrcp_CVE-2017-13281.c is the CVE-2017-13281 poc code.

$ mv avrcp_CVE-2017-13281.c blue-5.37/profiles/audio/avrcp.c

just replace blue-5.37/profiles/audio/avrcp.c with poc, and compile the source code on ubuntu 16.04， run bluetoothd manually, and paired my pixel xl with my laptop. Once paired, the attack payload will be sent automatically.

CVE-2018-9358 - CVE-2018-9361 are information disclosure, to see the leaked data which were sent back, you can use wireshark to capture the pcaps.



Note: just for research and test, not for illegal use.
