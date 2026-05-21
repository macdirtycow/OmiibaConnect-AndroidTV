# Android TV Bluetooth feasibility

Omiiba Connect for Android TV uses the same **Sony MDR RFCOMM** service as the macOS app and Sony | Sound Connect.

| Item | Value |
|------|--------|
| Service UUID | `96CC203E-5068-46ad-B32D-E316F5E069BA` |
| First packet | `CONNECT_GET_PROTOCOL_INFO` `{ 0x00, 0x00 }` |
| Reference | [OmiibaConnect Mac docs](https://github.com/macdirtycow/OmiibaConnect/blob/master/docs/apk-reference.md) |

## Phase 0: RFCOMM spike

Before relying on the full UI, run the built-in spike on your TV:

1. Pair **WH-1000XM3** (or XM4/XM5) in Android TV **Settings → Bluetooth**.
2. Install the APK.
3. Run:

```bash
adb shell am start -a dev.omiiba.connect.tv.RFCOMM_SPIKE -n dev.omiiba.connect.tv/.RfcommSpikeActivity
```

4. Select **Run RFCOMM + handshake test**.

| Result | Meaning |
|--------|---------|
| `RFCOMM spike succeeded` | This TV can host Omiiba Connect TV |
| `FAILED` / timeout | TV likely lacks Classic RFCOMM for third-party apps |

## Device notes (fill in when tested)

| Device | RFCOMM spike | Full app | Notes |
|--------|--------------|----------|-------|
| _Your TV_ | ☐ Pass ☐ Fail | ☐ | |
| NVIDIA Shield | ☐ | ☐ | Often works |
| Built-in Google TV | ☐ | ☐ | Varies by OEM |

## Why not run the Sony phone APK?

Sony | Sound Connect has no Leanback/TV launcher. It cannot be sideloaded as a TV app. Omiiba reads the same protocol (decompile reference only — see Mac `docs/sony-apk-analysis.md`).

## Legal

Not affiliated with Sony. Do not redistribute Sony APKs.
