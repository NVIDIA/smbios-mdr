project(
    'smbios-mdr',
    'cpp',
    version: '0.1',
    meson_version: '>=1.1.1',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
        'werror=true',
        'cpp_rtti=false',
    ],
)

boost_args = ['-DBOOST_ALL_NO_LIB', '-DBOOST_ASIO_DISABLE_THREADS']

boost_dep = dependency('boost')

sdbusplus_dep = dependency('sdbusplus')
phosphor_dbus_interfaces_dep = dependency('phosphor-dbus-interfaces')
phosphor_logging_dep = dependency('phosphor-logging')

# cpp_args_smbios: shared by src and test so test executables can compile main-app sources
cpp_args_smbios = boost_args
if get_option('dimm-dbus').allowed()
    cpp_args_smbios += ['-DDIMM_DBUS']
endif
if get_option('assoc-trim-path').allowed()
    cpp_args_smbios += ['-DASSOC_TRIM_PATH']
endif
if get_option('dimm-only-locator').allowed()
    cpp_args_smbios += ['-DDIMM_ONLY_LOCATOR']
endif
if get_option('slot-drive-presence').allowed()
    cpp_args_smbios += ['-DSLOT_DRIVE_PRESENCE']
endif
if get_option('tpm-dbus').allowed()
    cpp_args_smbios += ['-DTPM_DBUS']
endif
if get_option('firmware-inventory-dbus').allowed()
    cpp_args_smbios += ['-DFIRMWARE_INVENTORY_DBUS']
endif
if get_option('expose-firmware-component-name').allowed()
    cpp_args_smbios += ['-DEXPOSE_FW_COMPONENT_NAME']
endif
if get_option('custom-dbus-path').allowed()
    cpp_args_smbios += ['-DCUSTOM_DBUS_PATH']
endif
if get_option('cpu-dbus').allowed()
    cpp_args_smbios += ['-DCPU_DBUS']
endif
if get_option('cpu-dbus-chassisiface').allowed()
    cpp_args_smbios += ['-DCPU_DBUS_CHASSISIFACE']
endif
if get_option('copy-cpu-version-to-model')
    cpp_args_smbios += ['-DIS_COPY_CPU_VERSION_TO_MODEL=true']
else
    cpp_args_smbios += ['-DIS_COPY_CPU_VERSION_TO_MODEL=false']
endif
if get_option('dimm-location-code').allowed()
    cpp_args_smbios += ['-DDIMM_LOCATION_CODE']
endif
if get_option('expose-inventory')
    cpp_args_smbios += ['-DEXPOSE_FW_INVENTORY']
endif
if get_option('nvidia')
    cpp_args_smbios += ['-DNVIDIA']
endif
if get_option('procmod-dbus').allowed()
    cpp_args_smbios += ['-DPROCMOD_DBUS']
endif

subdir('include')
root_inc = include_with_build  # source + build so config.h is found
subdir('src')
subdir('service_files')

if get_option('tests').allowed()
    subdir('test')
endif

# gcovr-based HTML coverage when lcov/geninfo fails (e.g. "mismatched end line" in small GTest bodies).
# Uses gcovr.cfg in repo root (excludes subprojects/test); optional nvidia filter via openbmc-automation/nvidia-code-coverage/scripts/generate-nvidia-gcovr.sh
if get_option('b_coverage')
    _src = meson.project_source_root()
    _bld = meson.project_build_root()
    _coveragedir = join_paths(_bld, 'meson-logs', 'coveragereport')
    run_command('mkdir', '-p', _coveragedir, check: false)
    # Use wrapper script to remove duplicate .gcda files (same .cpp built in
    # multiple test executables causes gcov checksum mismatch / exit 5) then
    # run gcovr. MESON_BUILD_ROOT and MESON_SOURCE_ROOT are set by Meson.
    _coverage_script = join_paths(_src, 'scripts', 'run-coverage-gcovr.sh')
    run_target('coverage-gcovr', command: [find_program('sh'), _coverage_script])
endif
