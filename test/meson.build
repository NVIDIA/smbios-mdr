gtest_dep = dependency('gtest')
gtest_main_dep = dependency('gtest_main', required: true)
gmock_dep = dependency('gmock')

test_inc = include_directories('.')

test(
    'test_smbios_fixture',
    executable(
        'test_smbios_fixture',
        'test_smbios_fixture.cpp',
        dependencies: [gtest_main_dep, phosphor_logging_dep],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
)

main_app_sources = [
    'baseboard',
    'chassisCpu',
    'cpu',
    'dimm',
    'firmware_inventory',
    'mdrv2',
    'nvidia_firmware_inventory',
    'pcieslot',
    'system',
    'tpm',
]
mdrv2_dep_sources = [
    '../src/mdrv2.cpp',
    '../src/cpu.cpp',
    '../src/dimm.cpp',
    '../src/system.cpp',
    '../src/pcieslot.cpp',
    '../src/tpm.cpp',
    '../src/firmware_inventory.cpp',
    '../src/chassisCpu.cpp',
    '../src/nvidia_firmware_inventory.cpp',
]

test(
    'test_mdrv2',
    executable(
        'test_mdrv2',
        ['test_mdrv2.cpp'] + mdrv2_dep_sources,
        cpp_args: cpp_args_smbios,
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
    is_parallel: false,
)


test(
    'test_mdrv2_nvidia',
    executable(
        'test_mdrv2_nvidia',
        ['test_mdrv2.cpp'] + mdrv2_dep_sources,
        cpp_args: cpp_args_smbios + ['-DNVIDIA'],
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
    is_parallel: false,
)

test(
    'test_cpu_nvidia',
    executable(
        'test_cpu_nvidia',
        ['test_cpu.cpp', '../src/cpu.cpp'],
        cpp_args: cpp_args_smbios + ['-DNVIDIA'],
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
)


test(
    'test_mdrv2_no_procmod',
    executable(
        'test_mdrv2_no_procmod',
        ['test_mdrv2.cpp'] + mdrv2_dep_sources,
        cpp_args: cpp_args_smbios + ['-UPROCMOD_DBUS'],
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
    is_parallel: false,
)

test(
    'test_mdrv2_no_procmod_no_cpu',
    executable(
        'test_mdrv2_no_procmod_no_cpu',
        ['test_mdrv2.cpp'] + mdrv2_dep_sources,
        cpp_args: cpp_args_smbios + ['-UPROCMOD_DBUS', '-UCPU_DBUS'],
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
    is_parallel: false,
)

test(
    'test_mdrv2_no_procmod_no_cpu_no_dimm',
    executable(
        'test_mdrv2_no_procmod_no_cpu_no_dimm',
        ['test_mdrv2.cpp'] + mdrv2_dep_sources,
        cpp_args: cpp_args_smbios + [
            '-UPROCMOD_DBUS',
            '-UCPU_DBUS',
            '-UDIMM_DBUS',
        ],
        dependencies: [
            gtest_main_dep,
            gmock_dep,
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
        ],
        include_directories: [root_inc, test_inc],
    ),
    protocol: 'gtest',
    is_parallel: false,
)

if get_option('cpuinfo').allowed()
    test(
        'test_cpuinfo',
        executable(
            'test_cpuinfo',
            'test_cpuinfo.cpp',
            cpp_args: cpp_args_smbios,
            dependencies: [
                gtest_main_dep,
                gmock_dep,
                boost_dep,
                sdbusplus_dep,
                phosphor_logging_dep,
                phosphor_dbus_interfaces_dep,
            ],
            include_directories: [root_inc, test_inc],
        ),
        protocol: 'gtest',
    )

    test(
        'test_cpuinfo_utils',
        executable(
            'test_cpuinfo_utils',
            ['test_cpuinfo_utils.cpp', '../src/cpuinfo_utils.cpp'],
            cpp_args: cpp_args_smbios,
            dependencies: [
                gtest_main_dep,
                gmock_dep,
                boost_dep,
                sdbusplus_dep,
                phosphor_logging_dep,
                phosphor_dbus_interfaces_dep,
            ],
            include_directories: [root_inc, test_inc],
        ),
        protocol: 'gtest',
    )

    if get_option('cpuinfo-peci').allowed()
        test(
            'test_speed_select',
            executable(
                'test_speed_select',
                ['test_speed_select.cpp', '../src/cpuinfo_utils.cpp'],
                cpp_args: boost_args + peci_flag,
                dependencies: [
                    gtest_main_dep,
                    gmock_dep,
                    boost_dep,
                    peci_dep,
                    sdbusplus_dep,
                    phosphor_logging_dep,
                    phosphor_dbus_interfaces_dep,
                ],
                include_directories: [root_inc, test_inc],
            ),
            protocol: 'gtest',
        )
    endif
endif

foreach name : main_app_sources
    if name != 'mdrv2'
        if name == 'firmware_inventory'
            _sources = [
                'test_firmware_inventory.cpp',
                '../src/firmware_inventory.cpp',
                '../src/nvidia_firmware_inventory.cpp',
            ]
        elif name == 'baseboard'
            _sources = ['test_baseboard.cpp']
        else
            _sources = ['test_' + name + '.cpp', '../src/' + name + '.cpp']
        endif
        test(
            'test_' + name,
            executable(
                'test_' + name,
                _sources,
                cpp_args: cpp_args_smbios,
                dependencies: [
                    gtest_main_dep,
                    gmock_dep,
                    boost_dep,
                    sdbusplus_dep,
                    phosphor_logging_dep,
                    phosphor_dbus_interfaces_dep,
                ],
                include_directories: [root_inc, test_inc],
            ),
            protocol: 'gtest',
            is_parallel: name != 'firmware_inventory' and name != 'system',
        )
    endif
endforeach
