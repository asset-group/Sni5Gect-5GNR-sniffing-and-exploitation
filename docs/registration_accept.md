# Authentication Bypass: 5G AKA Bypass
This exploit is utilizing $I_8$ 5G AKA Bypass from paper [Logic Gone Astray: A Security Analysis Framework for the Control Plane Protocols of 5G Basebands](https://www.usenix.org/conference/usenixsecurity24/presentation/tu). Only the Pixel 7 phone with Exynos modem is being affected.
After receiving `Registation Request` from the UE, Sni5Gect injects the plaintext `Registration Accept` message with security header 4. The UE will ignore the wrong MAC and accept the `Registration Accept` message, reply with `Registration Complete` and `PDU Session Establishment Requests`. Since the core network receives such unexpected messages, it instructs the gNB to release the connection by sending the `RRC Release` message to terminate the connection immediately.
```conf
module = modules/lib_plaintext_registration_accept.so
```
Example output:

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/registration_accpet.png"/>
