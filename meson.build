project('upload_client', 'c', version: '0.1.0', subproject_dir: 'lib')

sources = files(
    'src/main.c',
)

dirs = include_directories(
    'src/include',
)

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'])
dtp_server_dep = dependency('dtp_server', fallback: ['dtp', 'dtp_server_dep'])
param_dep = dependency('param', fallback: ['param', 'param_dep'])
proto_c_dep = dependency('libprotobuf-c', fallback: ['protobuf-c', 'proto_c_dep'])
m_dep = meson.get_compiler('c').find_library('m', required : false)
deps = [csp_dep, param_dep, dtp_server_dep, proto_c_dep,  m_dep]

c_args = ['-DHOSTNAME="@0@"'.format(get_option('hostname'))]

executable(
    'upload_client',
    sources,
    include_directories: dirs,
    dependencies: deps,
    install: true,
    c_args: ['-g2', '-O0', '-Wall', '-Wextra'] + c_args,
    link_args: ['-ldl'],
)