# Sony | Sound Connect APK — referentie voor Android TV

Omiiba Connect TV gebruikt **dezelfde transportlaag als de officiële telefoon-app** voor WH-1000XM3. Dit document komt uit de gedecompileerde APK **9.5.0** (`com.sony.songpal.mdr`) in het Mac/Omiiba-project.

## APK decompileren (op je Mac)

```bash
cd /Users/leopold/SonyXM3Mac
brew install jadx   # eenmalig
# Zet de volledige APK in ~/Downloads (±63 MB, bv. Headphones-9.5.0.apk)
./scripts/analyze-sony-apk.sh
```

Output: `SonyXM3Mac/tools/apk/decompiled/sources/`

Kleine APKMirror-builds (~9 MB, `mdr_1.0.x`) bevatten **geen** protocol — gebruik de grote Headphones-apk.

## WH-1000XM3: Classic SPP (RFCOMM), niet LE-GATT

| Bron in APK | Betekenis |
|-------------|-----------|
| `pk/i.java` → `f27412a.b()` | SPP-service-UUID = **`96CC203E-5068-46ad-B32D-E316F5E069BA`** |
| `qj/b.java` | Verbindt XM3 via **SPP** (`sk.c` + `SocketConnection`) |
| `ServiceUuid.TANDEM_V2_HPC_SERVICE` | **`5b833e20-6bc7-4802-8e9a-723ceca4bd8f`** — BLE/GATT voor nieuwere modellen |
| `ConnectionMode.SPP` | Expliciet classic in o.a. `lk/o.java`, `leaudio/sequence/a.java` |

Omiiba TV gebruikt dus **de juiste UUID**; de fout `socket gesloten / read ret: -1` betekent dat de **TV het RFCOMM-kanaal niet opent**, niet dat we het verkeerde protocol kiezen.

## Connect-volgorde in Sony-app (`qj/b.java`)

1. Optioneel **GATT** als `ConnectionMode.AUTO` / GATT en LE-audio beschikbaar.
2. Voor XM3: **SPP** via `ym.b.a(...)` (obfuscated socket-laag).
3. Bij `SocketConnectionException`: fallback naar **GATT** als LE-UUID `443cce33-e85d-4b85-8d53-6e319ede53ae` aanwezig is.

WH-1000XM3 heeft **geen** GATT-fallback in de praktijk zoals XM5 — TV toont alleen **LE-WH-1000XM3** als apart koppelitem; dat is **niet** het SPP-pad uit de telefoon-app.

## MDR-commando's (handshake)

Zelfde als Mac/TV-app — `com/sony/songpal/tandemfamily/message/mdr/v1/table1/Command.java`:

1. `CONNECT_GET_PROTOCOL_INFO` `0x00`
2. `CONNECT_GET_CAPABILITY_INFO` `0x02`
3. `CONNECT_GET_SUPPORT_FUNCTION` `0x06`

Zie [OmiibaConnect Mac apk-reference.md](https://github.com/macdirtycow/OmiibaConnect/blob/master/docs/apk-reference.md).

## Wat de APK **niet** oplost voor Google TV

- Sony gebruikt een **eigen Bluetooth-stack** boven `BluetoothSocket`; wij gebruiken standaard Android RFCOMM.
- Veel TV’s staan **alleen A2DP** toe naar koptelefoons, geen third-party RFCOMM naar WH-1000XM3.
- **LE-GATT** (`5b833e20-…`) in de APK is een **ander transport** — apart project (zoals SonyHeadphonesClient BLE op Windows), niet XM3 SPP.

## Live vergelijking (aanbevolen)

Op een **Android-telefoon** met Sound Connect:

1. Ontwikkelaarsopties → **Bluetooth HCI snoop log** aan.
2. XM3 verbinden, één instelling wijzigen in Sound Connect.
3. `adb pull .../btsnoop_hci.log` → Wireshark → filter RFCOMM UUID `96cc203e-...`

Zie [packet-capture.md](https://github.com/macdirtycow/OmiibaConnect/blob/master/docs/packet-capture.md).

## Juridisch

Alleen voor interoperabiliteitsonderzoek. Decompile-output niet herdistribueren; Sony-APK niet in git.
