# DigestPlay

This tool allows you to play recorded digests or "amateur radio news" to DMR TG via BrandMeister's [Simple Application Protocol](https://wiki.brandmeister.network/index.php/Simple_External_Application).

Usage:

`cat sample.amb | ./digestplay --server-address [server address] --client-number [application account ID] --client-password [password used to connect] --source-id [DMR ID shown as a source] --group-id [TG ID]`

By default this tool uses DSD .amb format as an input source. You can switch to AMBE mode 33 using key `--mode33`.
AMBE mode 33 file may be produced by DVSI's usb3kcom.exe (supplied with DVSI USB-3000) or G4KLX's tool called wav2ambe (existing version should be patched to support mode 33)

How to produce .ambe file using DVSI's usb3kcom.exe:

`usb3kcom.exe -port COM3 460800 -enc -r 0x0431 0x0754 0x2400 0x0000 0x0000 0x6F48 sample.pcm sample.ambe`
