
conf_data = configuration_data()
conf_data.set('NVIDIA', get_option('nvidia'))

conf_header = configure_file(
  output: 'config.h',
  configuration: conf_data)
