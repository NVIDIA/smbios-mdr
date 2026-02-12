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

# gcovr-based HTML coverage (same approach as phosphor-post-code-manager, no scripts dir).
if get_option('b_coverage')
    gcovr = find_program('gcovr', required: false)
    if gcovr.found()
        gcovr_config_path = join_paths(meson.project_source_root(), 'gcovr.cfg')
        coverage_html_dir = join_paths(
            meson.current_build_dir(),
            'meson-logs',
            'coveragereport',
        )
        coverage_html_out = join_paths(coverage_html_dir, 'index.html')
        mkdir = find_program('mkdir')
        sh = find_program('sh')
        _cov_cmd = [
            sh,
            '-c',
            '@0@ -p @1@ && @2@ --config @3@ --root @4@ --html --html-details --branch -o @5@ && @2@ --txt-metric branch --print-summary --config @3@ --root @4@'.format(
                mkdir.path(),
                coverage_html_dir,
                gcovr.path(),
                gcovr_config_path,
                meson.project_source_root(),
                coverage_html_out,
            ),
        ]
        run_target('coverage-html-gcovr', command: _cov_cmd)
        run_target('coverage-gcovr', command: _cov_cmd)
    endif
endif
