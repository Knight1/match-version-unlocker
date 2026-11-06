# match-version-unlocker

An Atmosphere sysmodule for the Switch 1 that lets you share game updates with a Switch 2 through "Match Version with Local Users", for games where the Switch 2 otherwise cancels the update with error **2211-0520**.

## The problem

You want to keep your Switch 2 offline but some games show you an "Update required" error when you try to start them from a Switch 1 cartridge.  
When a Switch 1 on firmware 20.0.0 or newer offers an update to a Switch 2 for a game that has a Switch 2 Edition or other Switch 2 specific content, the Switch 2 aborts the session:

- Switch 2: error 2211-0520
- Switch 1: the update can't be performed

Confirmed with Switch 1 (21.1.0) to Switch 2 (21.1.0)
- Animal Crossing: New Horizons - SuperXCI - 01006F8002326000 (1.8.0 -> 3.0.3)
- The Legend of Zelda: Breath of the Wild - SuperXCI - 01007EF00011E800 (1.6.0 -> 1.9.0)
- The Legend of Zelda: Tears of the Kingdom - XCI - 0100F2C0115B6000 (1.0.0 -> 1.4.3)
- Super Mario Bros. Wonder - SuperXCI - 010015100B514800 (1.0.1 -> 1.2.0)
- Mario Kart 8 Deluxe - SuperXCI - 0100152000022000 (3.0.3 -> 3.0.5)

## Why it happens

The update offer contains an `ApplicationDeliveryInfo`, built and HMAC-signed by the sending
console's `ns`. Since 20.0.0 it includes the `ProperProgramExists` content meta attribute, which `ns`
reads with `ncm` `IContentMetaDatabase::GetAttributes`.

The receiving Switch 2 checks the sender's info with an extended `CanDeliverApplication` (`ns`
command 2019, documented on switchbrew). When the sender is a different platform, it only accepts:

    ContentMetaPlatform == 0 && ProperProgramExists == 0

A Switch 1 on 20.0.0+ reports `ProperProgramExists = 1` for these games, so the Switch 2 refuses.
Consoles on 18.x / 19.x never set the field, which is why they work.

## What this module does

It intercepts `ncm` for the `ns` process only and clears the `ProperProgramExists` bit in
`GetAttributes` results for applications and patches. `ns` then builds and signs the delivery info
itself with `ProperProgramExists = 0`, and the Switch 2 accepts the update. All other `ncm` commands
and all other processes are passed through unchanged.

The Switch 2 receives the regular Switch 1 version of the update.

## Requirements

- Switch 1 running Atmosphere, firmware 19.0.0 or newer. One build covers all of them,
  including firmware released after the build: there is no upper limit. Below 19.0.0 the module
  stays inactive.
- `ProperProgramExists` was added in 20.0.0. On 19.x the module runs but most likely has nothing
  to change.
- No other sysmodule that lists `ncm` in its `mitm.lst`. Two modules declaring the same service make
  Atmosphere abort at boot.

Only the sending Switch 1 needs the module. The Switch 2 stays unmodified.

## Installation

1. Download the latest release zip.
2. Extract it to the root of the SD card. It contains `atmosphere/contents/42000000162C63DA/`.
3. Reboot.

## Usage

On both consoles, highlight the game on the HOME Menu, press + (Options) and select
Software Update > Match Version with Local Users. Create the group on one console and join it with
the other. The console with the newer version sends the update.

## Risks

- It changes data the Switch 2 uses to decide compatibility. The Switch 2 gets the Switch 1 version
  of the update. If the game misbehaves on the Switch 2, or after buying the Nintendo Switch 2
  Edition, update the game on the Switch 2 over the internet.

## Building

Requires devkitPro with the Switch packages (`switch-dev`).

    git clone --recursive https://github.com/Knight1/match-version-unlocker
    cd match-version-unlocker
    make

The output is in `dist/`. `make release` also creates a zip.

The program ID is set once, as `title_id` in `config.json`; the Makefile reads it from there for the
`atmosphere/contents/<ID>/` folder.

The Makefile applies `patches/libstratosphere-allow-unknown-firmware.patch` to the Atmosphere-libs
submodule. Without it, libstratosphere aborts at startup on firmware it doesn't know, which would
leave `ns` waiting and hang the boot. With it, the module also runs on firmware released after the
build.

The Atmosphere libs need a newer libnx than the latest libnx release. The `libnx` submodule is
pinned to libnx master as of the Atmosphere release; the Makefile builds it into `build/` and uses it
instead of the libnx installed with devkitPro, which stays untouched.

GitHub Actions builds every push with the latest devkitPro image and attaches the zip to releases
created from `v*` tags.

## Credits ❤️

- switchbrew Community, for documenting `ns`, `ncm` and `ldn` 
- [SciresM](https://github.com/SciresM) for creating [Atmosphere-NX](https://github.com/atmosphere-nx/atmosphere) and libstratosphere
- [ldn_debug_logger](https://codeberg.org/TSRBerry/ldn_debug_logger), which this module grew out of.

## License

GPLv2, see `LICENSE`.
