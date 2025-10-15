project('upload_client', 'c', version: '0.1.0', subproject_dir: 'lib'
    default_options: [
		'c_std=gnu11',
		'b_lto=false',
		'default_library=static',
		'csp:packet_padding_bytes=42',
		'csp:buffer_count=15000',
		'csp:buffer_size=2048',
		'csp:conn_max=20',
		'csp:conn_rxqueue_len=1000',
		'csp:qfifo_len=1000',
		'csp:rdp_max_window=1000',
		'csp:port_max_bind=16',
		'csp:use_rtable=true',
		'param:have_fopen=true',
		'param:list_dynamic=true',
		'param:vmem_fram=false',
	],)

sources = files(
	'src/include/protobuf',
	'src/include/param',
	'src/include/vmem',
    'src/main.c',
    'src/vmem/vmem_upload_local.c',
	'src/vmem/vmem_dtp_server.c',
	'src/vmem/vmem_storage.c',
	'src/vmem/vmem_ring_buffer.c',
	'src/vmem/vmem_upload_local.c',
	'src/protobuf/metadata.pb-c.c',
	'src/protobuf/dtpmetadata.pb-c.c',
)

dirs = include_directories(
    'src/include',
)

csp_dep = dependency('csp', fallback: ['libcsp', 'csp_dep'])
param_dep = dependency('param', fallback: ['param', 'param_dep'])
dtp_server_dep = dependency('dtp_server', fallback: ['dtp', 'dtp_server_dep'])
proto_c_dep = dependency('libprotobuf-c', fallback: ['protobuf-c', 'proto_c_dep'])
m_dep = meson.get_compiler('c').find_library('m', required : false)
deps = [csp_dep,param_dep,dtp_server_dep,proto_c_dep,m_dep,brotli_dep]

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