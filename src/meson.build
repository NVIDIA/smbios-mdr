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

if get_option('dimm-location-code').allowed()
  cpp_args_smbios += ['-DDIMM_LOCATION_CODE']
endif

if get_option('cpu-dbus').allowed()
  cpp_args_smbios += ['-DCPU_DBUS']
endif

if get_option('cpu-dbus-chassisiface').allowed()
  cpp_args_smbios += ['-DCPU_DBUS_CHASSISIFACE']
endif
if get_option('procmod-dbus').allowed()
  cpp_args_smbios += ['-DPROCMOD_DBUS']
endif

if get_option('nvidia')
  cpp_args_smbios += ['-DNVIDIA']
endif

if get_option('expose-inventory')
  cpp_args_smbios += ['-DEXPOSE_FW_INVENTORY']
endif

if get_option('copy-cpu-version-to-model')
  cpp_args_smbios += ['-DIS_COPY_CPU_VERSION_TO_MODEL=true']
else
  cpp_args_smbios += ['-DIS_COPY_CPU_VERSION_TO_MODEL=false']
endif

executable(
  'smbiosmdrv2app',
  'mdrv2.cpp',
  'mdrv2_main.cpp',
  'cpu.cpp',
  'chassisCpu.cpp',
  'dimm.cpp',
  'system.cpp',
  'pcieslot.cpp',
  'firmware.cpp',
  'tpm.cpp',
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
