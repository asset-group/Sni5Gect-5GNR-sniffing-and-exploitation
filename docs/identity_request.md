# Fingerprinting: Identity Request
Demonstrates a fingerprinting attack by injecting an `Identity Request` message after receiving a `Registration Request`. If the UE accepts, it responds with an `Identity Response` containing its SUCI information.

```conf
module = modules/lib_identity_request.so 
```

Example output:

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/identity_request_output.png"/>