# Downgrade: Registration Reject
Utilizes the $TC_{11}$ attack from the paper [Never Let Me Down Again: Bidding-Down Attacks and Mitigations in 5G and 4G](https://dl.acm.org/doi/10.1145/3558482.3581774).  Injects a `Registration Reject` message after receiving a `Registration Request` from the UE, causing it to disconnect from 5G and downgrade to 4G. Since the base station may not be aware of the disconnection, it may keep sending the messages such as `Security Mode Command`, `Identity Request`, `Authentication Request`, etc.

```conf
module = modules/lib_dg_registration_reject.so 
```
Example Output:

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/registration_reject_output.png"/>
