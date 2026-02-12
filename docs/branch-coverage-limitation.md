# Branch coverage limitations: smbios-mdr

This document records branch coverage after UT-only improvements and documents remaining limitations (no production code changes). Coverage measured with gcovr (branch view), repo `src/` (and `include/` where applicable), after running unit tests with `b_coverage=true`.

**How to generate coverage:**
- **Local (fewer branches, higher %):** `meson setup build -Db_coverage=true -Dcpuinfo=disabled -Dsmbios-ipmi-blob=disabled`, then **`ninja -C build test && ninja -C build coverage-html-gcovr`**. Report: `build/meson-logs/coveragereport/index.html`.
- **CI-equivalent (matches CI branch count and %):** Use the same build options as CI (defaults: cpuinfo and smbios-ipmi-blob enabled), then Meson’s native coverage target: **`meson setup build -Db_coverage=true && ninja -C build test && ninja -C build coverage`**. Use the coverage output path that Meson prints (e.g. `build/meson-logs/coveragereport` or the path from `ninja -C build coverage`). If your CI uses a dedicated coverage build dir (e.g. `build/coverage`), use that instead of `build`.

**Why the overall branch % may stay the same after adding tests:** The total branch count (~1084) is large; each new test often covers only a few branches. So 65.7% → 66% might require a dozen or more new covered branches. The biggest share of uncovered branches is in `mdrv2.cpp` (D-Bus, file state, validation) and in D-Bus/exception paths that are hard to hit without production changes. To see the effect of new tests, run **`ninja -C build test && ninja -C build coverage-html-gcovr`** in one go after rebuilding; open the HTML report and check per-file branch coverage for the modules you changed.

**After running coverage:** Open the HTML report (branch view), then for each file section below replace "See coverage report" with the actual values: **Total branches**, **Covered**, **Uncovered**, **Branch coverage %**.

**Overall (repo-only):** Branch counts and percentage — see coverage report after running the above.

**Target: 80% branch coverage** — Current coverage is ~68% (e.g. 845/1245 branches). Reaching 80% (~996 branches) requires covering ~150 more branches. Many of these are in `mdrv2.cpp` (D-Bus, file I/O, directory state) and are difficult to cover without production code changes (e.g. injectable bus/file) or integration tests. See "Scope for improvement" and "Remaining" below for what is feasible with unit tests only.

**CI vs local branch coverage** — CI (openbmc-build-scripts `unit-test.py`) reconfigures with **only** `-Db_coverage=true` and `-Dtests=enabled`; it does **not** pass `-Dcpuinfo=disabled` or `-Dsmbios-ipmi-blob=disabled`, so **CI builds with default options** (cpuinfo and smbios-ipmi-blob enabled). That pulls in phosphor-ipmi-blobs and more code, so the total branch count is higher (e.g. ~1407 in CI vs ~1245 locally with those options disabled) and the extra code often has little unit-test coverage, which lowers the overall branch %. The script prefers **`coverage-html-gcovr`** when the project defines it (smbios-mdr does), so CI uses the same gcovr + `gcovr.cfg` (filters, exclude-throw-branches, etc.) as a local `ninja coverage-html-gcovr` run. To align CI percentage with a local “reduced build”, CI would need the same meson options (e.g. `-Dcpuinfo=disabled -Dsmbios-ipmi-blob=disabled`), which would require changes in openbmc-build-scripts or a package-specific coverage profile.

---

## Why is branch coverage relatively low?

1. **D-Bus and external I/O** — A large share of branches are in code that calls D-Bus (mapper, properties, signals) or filesystem. Unit tests use a mock bus and temp files; branches that depend on real D-Bus failures, mapper errors, or specific bus behavior are not exercised without production changes (e.g. injectable bus).

2. **Compiler-generated and inlined code** — gcov counts branches in exception handling, destructors, and inlined/template code. We use gcovr `exclude-throw-branches = yes` and `exclude-unreachable-branches = yes` in `gcovr.cfg` to reduce noise; some arcs remain and are documented per file.

3. **Build and configuration options** — Some code is built only with options like `cpuinfo`, `cpuinfo-peci`, or `smbios-ipmi-blob` enabled, or under `DIMM_DBUS` / `TPM_DBUS` etc. The standard test build disables cpuinfo and smbios-ipmi-blob, so those branches are excluded from the reported coverage.

4. **Stateful / protocol-dependent paths** — MDRV2 and inventory logic have branches that depend on specific MDR file layout, directory state, or D-Bus object presence. Covering every combination would require more elaborate stubs or integration tests.

---

## Scope for improvement (no source changes)

- **Done:** Additional unit tests were added to cover more branches where possible:
  - **system.cpp:** `version()` with non-printable BIOS version when the MDRV2 table file path is writable (covers `smbiosFile.good()` true path: truncate file and return "No BIOS Version").
  - **dimm.cpp:** `updateFormFactor()` with a key not in `dimmFormFactorMap` (defaults to RDIMM); `dimmSize()` with new-version encoding (size bit 0x8000 set, else branch); `dimmDeviceLocator()` when config is empty (CPU/DIMM substring parsing, socket/slot from "CPU0"/"DIMM_A"); `dimmType()`/`dimmMedia()` with unknown type (Unknown); `dimmManufacturer()` "NO DIMM" path; `dimmTypeDetail()` with multiple bits set in `typeDetail` (loop over bits).
  - **cpu.cpp:** `family()` unknown (not in table); `family()` with Processor Family 2 Indicator (0xfe) and `family2` in `family2Table` (e.g. ARMv7); `functional(false)` when status mask != 1; core/thread count from extended fields.
  - **chassisCpu.cpp:** `infoUpdate()` with socket populated but `functional(false)` (status bit 6 set, (status & 0x07) != 1).
  - **pcieslot.cpp:** `pcieLaneSize()` unknown width → lanes(0).
  - **tpm.cpp:** vendor non-printable → '.' replacement.
  - **mdrv2.cpp:** `sendDataInformation()` with only one of dataLen/dataVer/timestamp changed (single-field change branches); getDataInformation invalid index throws; sendDirectoryInformation same version; directoryEntries missing file. dimm: size 0x7fff uses dimmSizeExt. cpu: characteristics with capability bit. firmware_inventory: path fallback when id trims empty.
  - **tpm.cpp:** `tpmFirmwareVersion()` with specMajor neither tpmMajorVersion1 nor tpmMajorVersion2 (empty version string).
  - **dimm.cpp:** `dimmDeviceLocator()` with non-empty bankLocator (bankLocator + " " + deviceLocator) and deviceLocator "DIMM_C" so single-letter slot from regex.
  - **cpu.cpp:** `infoUpdate()` with coreCount and threadCount both &lt; 0xFF (non-extended coreCount/threadCount path).
- **Remaining:** Further gains without source changes are limited to adding more tests that use existing public APIs and mock/temp data; D-Bus exception paths and some MDRV2 state branches would require injectable dependencies or more complex stubs.

---

## src/cpuinfo_utils.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.
- **Try/catch:** `updateOsState` — catch `sdbusplus::exception::InvalidEnumString` (invalid enum string); covered by tests that call `updateOsState("NoSuchStatus")` and `updateOsState("InvalidStatusThatDoesNotMatchAnyEnum")`.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling, dtors, template/lambda code | ~5–10% | gcov branch arcs in generated code. |
| D-Bus / external | `subscribeToProperty`, `hostStateSetup`, getProperty/match callbacks | ~15–20% | Requires real D-Bus or injectable connection; not feasible in UT without prod change. |
| Static / internal | `updateHostState` (called from public API; branches covered via `updatePowerState`/`updateBiosDone`/`updateOsState`) | — | Covered via public API. |

**Fixable by UT (done):** Invalid OS state string (catch path), power state transitions, biosDone/osState branches, callback invocation on state change, no-callback when state unchanged.

**Real limitation (no prod change):** D-Bus subscription and property-read paths; compiler-generated arcs.

---

## src/system.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.
- **Try/catch:** `getService` — catch `sdbusplus::exception_t` (mapper call failure); returns empty string. Tests exercise `version()` which calls `setProperty` → `getService`; catch path requires D-Bus to throw (mock or real failure).

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling, stream/formatting | ~5% | Estimated. |
| D-Bus / external | `getService` try/catch, `setProperty` (bus.call_noreply) | ~15–20% | Would need injectable bus or way to make mapper call throw in UT. |
| Filesystem | `version()` non-printable path: open/truncate file, `smbiosFile.good()` | ~5% | Partially covered by tests with invalid path; some branches depend on open success/failure. |

**Fixable by UT (done):** `uuid()` null/valid data, default UUID; `version()` null/valid BIOS data, "No BIOS Version", non-printable char path, file open failure path, and non-printable with writable path (truncate file, `smbiosFile.good()` true).

**Real limitation (no prod change):** getService exception path; setProperty on real bus.

---

## src/dimm.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.
- **Try/catch:** `dimmDeviceLocator` — catch `sdbusplus::exception_t` when parsing socket number; `std::stoi` throws `std::invalid_argument`/`std::out_of_range`, so this catch may be unreachable (type mismatch) unless sdbusplus exception hierarchy extends those.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling, operators | ~5% | Estimated. |
| D-Bus / external | `socket()`, `slot()`, property set, association calls | ~10–15% | Some paths require D-Bus; many branches covered via mock bus in tests. |
| Conditional compilation | `DIMM_ONLY_LOCATOR`, `DIMM_DBUS`, etc. | Build-dependent | Different branches per build options. |

**Fixable by UT (done):** memoryInfoUpdate branches (dataIn null, dimmNum loop), size/extendedSize, present/functional, device locator parsing (slot regex, socket number), `updateFormFactor()` key not in map (RDIMM default), `dimmSize()` new-version encoding (0x8000 bit set).

**Real limitation (no prod change):** D-Bus setter branches when bus behaves differently; compiler arcs.

---

## src/cpu.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling, generated code | ~5% | Estimated. |
| D-Bus / external | Property and association updates | ~10–15% | Mock bus in tests covers many paths. |

**Fixable by UT (done):** SMBIOS parsing branches, socket/version/model branches testable via public API.

**Real limitation (no prod change):** D-Bus-specific branches; compiler arcs.

---

## src/pcieslot.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling | ~5% | Estimated. |
| D-Bus / external | Property updates, associations | ~10% | Mock bus. |

**Fixable by UT (done):** Slot table parsing, presence/lane branches where testable.

**Real limitation (no prod change):** D-Bus; compiler arcs.

---

## src/tpm.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling | ~5% | Estimated. |
| D-Bus / external | TPM property/interface (when TPM_DBUS enabled) | ~10% | Build option and D-Bus. |

**Fixable by UT (done):** SMBIOS TPM type parsing, conditional branches testable via storage.

**Real limitation (no prod change):** D-Bus; compiler arcs.

---

## src/firmware_inventory.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.
- **Try/catch:** D-Bus exception around inventory/version read; catch returns or logs.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| D-Bus / external | try/catch on D-Bus call | ~15% | Would need D-Bus to throw in UT. |
| Compiler-generated arcs | Exception handling | ~5% | Estimated. |

**Fixable by UT (done):** Type 45 / firmware parsing branches testable with table data.

**Real limitation (no prod change):** D-Bus exception path; compiler arcs.

---

## src/nvidia_firmware_inventory.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| D-Bus / external | NVIDIA-specific inventory interface | ~10–15% | Mock or integration. |
| Compiler-generated arcs | Exception handling | ~5% | Estimated. |

**Fixable by UT (done):** Parsing and conditional branches testable with table data.

**Real limitation (no prod change):** D-Bus; compiler arcs.

---

## src/chassisCpu.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| D-Bus / external | Chassis/CPU association and properties | ~10% | Mock bus. |
| Compiler-generated arcs | Exception handling | ~5% | Estimated. |

**Fixable by UT (done):** Table parsing and index branches.

**Real limitation (no prod change):** D-Bus; compiler arcs.

---

## src/mdrv2.cpp

- **Total branches / Covered / Uncovered / %:** See coverage report.
- **Try/catch:** Multiple D-Bus and validation paths; throws `sdbusplus::xyz::openbmc_project::Smbios::MDR_V2::Error::*` and catch blocks for D-Bus exceptions.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated arcs | Exception handling, throw/catch | ~5–10% | Many throw/catch branches. |
| D-Bus / external | getService, find anchor, commit, property updates | ~20–25% | Requires D-Bus to throw or return specific values. |
| Validation / error | InvalidId, InvalidData, entry validation branches | Partially covered | Tests can trigger some via invalid input. |

**Fixable by UT (done):** Validation branches (invalid size, invalid entry, etc.) where testable via public API; some error paths.

**Real limitation (no prod change):** D-Bus exception catch blocks; some validation only reachable with specific MDR state.

---

## include/baseboard.hpp

- **Total branches / Covered / Uncovered / %:** See coverage report (header included in test_baseboard).
- Baseboard is header-only; test_baseboard exercises constructor and accessors with table data.

| Limitation | Affected functions/area | Approx. % of file's branches not covered | Unblocked by / Reason |
|------------|-------------------------|------------------------------------------|------------------------|
| Compiler-generated / inline | Operators, destructors, inline accessors | ~10–15% | Header inlined in multiple TUs. |

**Fixable by UT (done):** Constructor branches, board type, contained handles (via test_baseboard).

**Real limitation (no prod change):** Compiler-generated and inline branches.

---

## Excluded from coverage (gcovr.cfg)

- `mdrv2_main.cpp`, `cpuinfo_main.cpp` — application entry points; not run by unit tests.
- `speed_select.cpp`, `sst_mailbox.cpp` — built only when `cpuinfo` and `cpuinfo-peci` are enabled; PECI/hardware-dependent (real limitation for branch coverage in standard test build).
