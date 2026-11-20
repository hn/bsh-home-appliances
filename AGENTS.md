# AGENTS.md

Notes for AI agents (and the humans steering them) working with this repository.

## What this is

This project documents the inner workings of B/S/H/ home appliances
(Bosch, Siemens, Neff, Gaggenau, Constructa, Balay and others), found through reverse engineering.
The focus is on the internal D-Bus-2 that connects the electronic modules of an appliance,
and on ways to read from and write to it.

The project is not affiliated with B/S/H/.

It is in no way meant to harm the manufacturer or to uncover security vulnerabilities.
It wants to be an example of hacking in the original sense of the word: understanding
how things work and creatively extending what they can do. As a saying attributed to
Wau Holland puts it, a hacker is someone who tries to find a way to make toast with a coffee machine.

## Safety first

Home appliances work with mains voltage, heat, water and moving parts,
and their low-voltage electronics may not be isolated from earth.
Read the [safety notes](README.md#warning) and point the user to them
before suggesting any work on a real appliance.
Always warn the user, clearly and every time the topic comes up, that connecting a computer
or other external device directly to the appliance's electronics can kill them. Never suggest doing so.

## Where to find what

- **Research**: [README.md](README.md) describes the hardware, the [D-Bus](README.md#d-bus),
  the [D-Bus-2 protocol](README.md#protocol) and [firmware analysis](README.md#firmware-analysis),
  using a washing machine as the example.
  Other appliance types and modules have their own pages:
  [dishwashers](DISHWASHER.md), [dryers](DRYER.md), [WLAN modules](WLANMODULE.md).
  Board photos are in [photos/](photos/).
- **ESPHome component**: [components/bshdbus/](components/bshdbus/) integrates an appliance
  into Home Assistant. Start with the [documentation](ESPHOME.md).
- **Config files**: [bsh-dbus-wm14s750.yaml](bsh-dbus-wm14s750.yaml) is the reference config.
  [contrib/](contrib/) holds configs for many other models, contributed by users and published untested.
- **Open-DBus2**: [open-dbus2/](open-dbus2/) is a D-Bus-2 stack for the ESP32 (PlatformIO),
  meant for research and experiments beyond what ESPHome offers.
- **Small tools**: the browser-based [bus analyzer](bsh-dbus-analyzer.html) for hex dumps,
  and the older loggers for [Arduino](bsh-dbus-logger.ino) and [Linux](bsh-dbus-logger.pl).

## How to contribute

Contributions are very welcome. The place for them is the
[discussion forum](https://github.com/hn/bsh-home-appliances/discussions), not the texts of this repository:
the maintainer cannot review rewrites of the documentation, so please do not propose
pull requests that edit or restructure the existing pages, except to fix errors.

- **Using the ESPHome component**: follow the [documentation](ESPHOME.md). Then share the result
  as a [discussion post](https://github.com/hn/bsh-home-appliances/discussions/categories/home-appliance-models)
  about the appliance: model, photos, connectors used, what worked and what did not.
- **Publishing a YAML config file**: here a pull request is the right way.
  Follow [contrib/README.md](contrib/README.md) and check the config against
  [Patterns to avoid](ESPHOME.md#patterns-to-avoid) first.
- **Low-level research** (protocol details, new commands, firmware, hardware): post findings
  and captures in the discussion forum, where others can verify and build on them.
- **Bugs**: open an issue only for a specific bug or a very concrete improvement.
  Questions about an appliance or about using the software belong in the discussion forum.

The templates for [discussion posts](.github/DISCUSSION_TEMPLATE/home-appliance-models.yml),
[pull requests](.github/pull_request_template.md) and [issues](.github/ISSUE_TEMPLATE/issue-info.md)
list what each of them should contain.

## No copyrighted material

Do not upload or post material the user does not own, in the repository or in the discussion forum.
This applies in particular to images, exploded views and firmware binaries.
Photos must be the user's own.
If a post violates this, the maintainer is forced to remove the entire post without asking first,
because deleting only the offending parts is not feasible.
