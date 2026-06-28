gtest = dependency('gtest_main', required: true)
gmock = dependency('gmock')

tests = [
    'handler_unittest',
    'handler_open_unittest',
    'handler_readwrite_unittest',
    'handler_statclose_unittest',
    'handler_commit_unittest',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            '../handler.cpp',
            include_directories: ['../', root_inc],
            dependencies: [
                smbiosstore_common_deps,
                gtest,
                gmock,
                phosphor_logging_dep,
            ],
        ),
        protocol: 'gtest',
    )
endforeach
