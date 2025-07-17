# Downgrade: Authentication Replay
This exploit corresponds to `CVD-2024-0096`. It is the most complex exploit we have, which involves two stages, sniffing and replaying. Then during the relaying stage, it requires sniffing and injecting at multiple different states.

1. Sniffing: Capture a legitimate Authentication Request from the base station to the UE.
```conf
module = modules/lib_dg_authentication_request_sniffer.so 
```

2. Replaying: Update `shadower/modules/dg_authentication_replay.cc` with the captured MAC-NR values. Rebuild the module:
```bash
ninja -C build
```

Then load the module:
```conf
module = modules/lib_dg_authentication_replay.so
```

Upon receiving `Registration Request` from the UE, Sni5Gect replays the captured `Authentication Request` message to the target UE. Upon receiving the replayed `Authentication Request` message, the UE replies with `Authentication Failure` message with cause `Synch Failure` and starts the timer T3520. Then Sni5Gect update its RLC and PDCP sequence number accordingly and replays the `Authentication Request` message for a few more times. Eventually, after multiple attempts and timer T3520 expires, the UE deems that the network has failed the authentication check. Then it locally releases the communication and treats the active cell as barred. If no other 5G base station is available, then the UE will downgrade to 4G and persists in downgrade status up to 300 seconds according to the specification 3GPP TS 24.501 version 16.5.1 Release 16 `5.4.1.2.4.5 Abnormal cases in the UE`. (Some phone may stay in downgrade status for much longer time).

In the example output, we can identify the UE replies the `Authentication Failure` message two times in the following screenshot.

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/authentication_replay_output.png"/>