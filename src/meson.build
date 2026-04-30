# cpp_args_smbios is defined in root meson.build (shared with test/)

if get_option('publish-inventory').allowed()
    cpp_args_smbios += ['-DPUBLISH_INVENTORY']
endif

executable(
    'smbiosmdrv2app',
    'mdrv2.cpp',
    'mdrv2_main.cpp',
    'cpu.cpp',
    'dimm.cpp',
    'system.cpp',
    'pcieslot.cpp',
    'tpm.cpp',
    'firmware_inventory.cpp',
    'chassisCpu.cpp',
    'nvidia_firmware_inventory.cpp',
    cpp_args: cpp_args_smbios,
    dependencies: [
        boost_dep,
        sdbusplus_dep,
        phosphor_logging_dep,
        phosphor_dbus_interfaces_dep,
    ],
    implicit_include_directories: false,
    include_directories: root_inc,
    install: true,
)

if get_option('cpuinfo').allowed()
    cpp = meson.get_compiler('cpp')
    # i2c-tools provides no pkgconfig so we need to find it manually
    i2c_dep = cpp.find_library('i2c')

    peci_dep = []
    peci_flag = []
    peci_files = []
    if get_option('cpuinfo-peci').allowed()
        peci_flag = '-DPECI_ENABLED=1'
        peci_dep = dependency('libpeci')
        peci_files = ['speed_select.cpp', 'sst_mailbox.cpp']
    endif

    executable(
        'cpuinfoapp',
        'cpuinfo_main.cpp',
        'cpuinfo_utils.cpp',
        peci_files,
        cpp_args: boost_args + peci_flag,
        dependencies: [
            boost_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            phosphor_dbus_interfaces_dep,
            i2c_dep,
            peci_dep,
        ],
        implicit_include_directories: false,
        include_directories: root_inc,
        install: true,
    )
endif

if get_option('smbios-ipmi-blob').allowed()
    subdir('smbios-ipmi-blobs')
endif
