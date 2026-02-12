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
        )
    endif
endforeach
