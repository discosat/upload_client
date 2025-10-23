project('upload_client', 'c', version: '0.1.0', subproject_dir: 'lib',
    default_options: [
		'c_std=gnu11',
		'b_lto=false',
		'default_library=static',
		'param:have_fopen=true',
		'param:list_dynamic=true',
		'param:vmem_fram=false',
	],
)

sources = files(
    'src/main.c',
)

dirs = include_directories(
    'src/include',
    'src/include/vmem',
)

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'])
m_dep = meson.get_compiler('c').find_library('m', required : false)
dtp_client_dep = dependency('dtp_client', fallback: ['dtp', 'dtp_client_dep'], required: true)
deps = [csp_dep, dtp_client_dep, m_dep]

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