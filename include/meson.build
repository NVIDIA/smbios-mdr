
conf_data = configuration_data()

if get_option('firmware-component-name-bmc') != ''
conf_data.set_quoted('FIRMWARE_COMPONENT_NAME_BMC', get_option('firmware-component-name-bmc'))
endif

if get_option('firmware-component-name-bios') != ''
conf_data.set_quoted('FIRMWARE_COMPONENT_NAME_BIOS', get_option('firmware-component-name-bios'))
endif

if get_option('firmware-component-name-tpm') != ''
conf_data.set_quoted('FIRMWARE_COMPONENT_NAME_TPM', get_option('firmware-component-name-tpm'))
endif

if get_option('platform-prefix') != ''
conf_data.set_quoted('PLATFORM_PREFIX', get_option('platform-prefix'))
endif

conf_header = configure_file(
  output: 'config.h',
  configuration: conf_data)
