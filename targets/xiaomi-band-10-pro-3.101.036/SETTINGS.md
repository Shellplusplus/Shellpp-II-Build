# Settings injection: 3.101.036

Static firmware evidence; device validation is pending.

- AP SHA-256: `662d67f5e247e31e194d3161024890ba93b9d29d70b290fadb9aac8ce8ec3c81`.
- Startup copies initialized data from alias `0x2cd98698` to RAM `0x200c7750..0x200eb39c`. Template file offset `0xce40c4` maps to `0x200d317c`.
- Settings creation `0x0c544ea1` references that template and copies 40 bytes through `0x0ca42c91` to list+0x5c.
- Live slots: `0x2010fd68` and `0x2010fd60`; destruction clears both. Refresh: `0x0ca43439`.
- Original template: `[0, 0x0c544875, 0x0c545039, 0x0c544941, 0, 0x0c5448c1, 0x0c544b09, 0x0c544b19, 0, 0]`.
- Count is 10 minus the variant byte from `0x0c49e779`. Wrappers accept original counts 9/10 and append one item without shifting existing indices.
- Raw row creation/configuration: `0x0c4a77fd` / `0x0c4a7849`. Null subtitle selects the single-label path at `0x0c4a7ab2`. Settings icon is 64x64; Launcher remains 91x91.
- Cache lookup `0x0ca40c85` compares a byte-sized type; custom type is 0xfe.
- Lua uses Settings command `0x5351000b`, BUILD `0x03650901`; Both publishes Launcher then injects Settings, Settings skips Launcher publication.
- Template mismatch and transitional live slots are rejected before writes; module remains resident after callback publication.

Host row tests cover delegation, 9/10 counts, null subtitle, repeated binds and click filtering. Template tests cover mismatch rejection, preserved slots, repeated installation and partial-publication guards. They do not prove UI serialization or live-object validity on the device.

Device checks still required: all three modes, Settings click, scrolling/recycling, re-entry, already-open Settings, language refresh and reboot. Resume directly bounds-checks original indices and may log for the appended item, as in 043; runtime impact remains unverified.
