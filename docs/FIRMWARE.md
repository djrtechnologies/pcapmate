# Firmware supplied by the user

No firmware is bundled or downloaded automatically. The actual validation used the generic IT9303 bridge firmware 1.4.0.0 named `dvb-usb-it9303-01.fw`, originally obtained for the internal app from https://github.com/osmc/dvb-firmware-osmc .

Validated blob SHA-256: `5213a5a38872661277a2cc1b2dfdfe88faf06f41205f460f3b51857f0568b484`

The upstream driver also documents extracting a vendor-specific bridge firmware from `liba3_phy_sony.so` in the manufacturer's Android application: https://github.com/koreapyj/cxd28xx#firmware . Generic firmware can have different ATSC 3.0 performance. No APK or proprietary library is required to build this source. Obtain any firmware separately under its applicable terms, then pass its absolute path with `--firmware`. Do not assume a repository hosting a blob grants permission to redistribute it.

The bridge may retain loaded firmware until it loses power; provide the firmware path anyway so a freshly attached/cold receiver can initialize.
